"""Visual Studio のプロジェクトとフォルダ構成を同期するツール。

■ 何をするか
    1. Source/ 以下の .h / .cpp を走査し、.vcxproj の ClInclude / ClCompile を作り直す
    2. .vcxproj.filters を、ディスクのフォルダ構成そのままに作り直す
       このとき C++ だけでなく、シェーダー(SlangTask / FxCompile / CustomBuild)や
       その他(None)の項目も同じ規則でフォルダへ割り当てる

■ なぜ必要か
    ファイルを手で追加していくと .filters が実際のフォルダと少しずつずれ、
    ソリューションエクスプローラーのツリーが崩れていく。
    「ディスクの構成こそが唯一の正」と決めて毎回生成し直せば、ずれようがない。

■ C++ とシェーダーで扱いが違う理由
    .h / .cpp は「Source 以下にあるものはすべてビルド対象」でよいので、ディスクを走査する。
    シェーダーや CustomBuild は「どれをビルド対象にするか」が意図的な選択で、
    ビルド設定（プロファイル・エントリポイント等）も項目ごとに違う。
    そのため一覧は .vcxproj のものをそのまま尊重し、フォルダ割り当てだけを直す。

■ 壊さないための約束
    - 既存のフィルタ GUID は引き継ぐ（変えると VS がツリーの展開状態を忘れる）
    - ファイルごとの設定（Pch.cpp の PrecompiledHeader 等）はそのまま残す
    - .vcxproj 側で触るのは ClInclude / ClCompile の ItemGroup だけ
    - 改行コードは CRLF、BOM 付き UTF-8 のまま書き戻す
    - プロジェクトから外れるファイルがあれば必ず一覧表示する

■ 使い方
    python Tools/SyncProjectFiles.py            変更を書き込む
    python Tools/SyncProjectFiles.py --check    差分の有無だけを表示する（書き込まない）
"""

import io
import os
import re
import sys
import hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "Source")
PROJ = os.path.join(ROOT, "DX12Framework.vcxproj")
FILTERS = os.path.join(ROOT, "DX12Framework.vcxproj.filters")

# -------------------------------------------------------------------------------
# 走査から除外するフォルダ
#
# フォルダ「名」で弾くと、Utility/Debug のような正当なフォルダまで消えてしまう。
# Source からの相対パスで、除外したい場所だけを正確に指定する。
# -------------------------------------------------------------------------------
EXCLUDE_RELATIVE_DIRS = {
    os.path.join("Engine", "Shader", "Compiled"),   # コンパイル済みシェーダーの出力先
}

SOURCE_EXT = (".cpp",)
HEADER_EXT = (".h", ".hpp", ".inl")

# ディスク走査で一覧を決める項目（C++）
SCANNED_TAGS = ("ClInclude", "ClCompile")

# 一覧は .vcxproj を尊重し、フォルダ割り当てだけを直す項目
KEPT_TAGS = ("SlangTask", "CustomBuild", "FxCompile", "None")


# -------------------------------------------------------------------------------
# 入出力
# -------------------------------------------------------------------------------
def read(path):
    # newline="" を付けないと CRLF が LF に潰れ、ファイル全体が差分になる
    with io.open(path, encoding="utf-8-sig", newline="") as f:
        return f.read()


def write(path, text):
    with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
        f.write(text)


def stable_guid(name):
    """フィルタ名から決まった GUID を作る。実行のたびに変わらないようにする。"""
    h = hashlib.md5(name.encode("utf-8")).hexdigest()
    return "%s-%s-%s-%s-%s" % (h[0:8], h[8:12], h[12:16], h[16:20], h[20:32])


# -------------------------------------------------------------------------------
# ディスクの走査
# -------------------------------------------------------------------------------
def collect_cpp_files():
    """Source/ 以下のソースとヘッダを、ROOT からの相対パスで集める。"""
    sources, headers = [], []

    for base, dirs, files in os.walk(SRC_DIR):
        # ドット始まりのフォルダ（.vs など）と、明示的に除外した場所だけを飛ばす
        dirs[:] = [
            d for d in dirs
            if not d.startswith(".")
            and os.path.relpath(os.path.join(base, d), SRC_DIR) not in EXCLUDE_RELATIVE_DIRS
        ]

        for name in files:
            lower = name.lower()
            rel = os.path.relpath(os.path.join(base, name), ROOT)

            if lower.endswith(SOURCE_EXT):
                sources.append(rel)
            elif lower.endswith(HEADER_EXT):
                headers.append(rel)

    # 並びを固定する。順序が揺れると意味のない差分が出る
    return sorted(sources, key=str.lower), sorted(headers, key=str.lower)


def collect_includes(text, tag):
    """.vcxproj から、指定タグの Include 一覧を出現順で取り出す。"""
    return re.findall(r'<%s Include="([^"]+)"' % tag, text)


def filter_of(rel_path):
    """ファイルのパスから、所属するフィルタ名を求める。

    Source\\Engine\\EditorUI\\Core\\Types.h  ->  Engine\\EditorUI\\Core
    Source\\Pch.h                           ->  ""（ルート直下）

    Source 以外の場所にあるファイルは、パスの先頭フォルダをそのまま使う。
    """
    rel = rel_path
    prefix = "Source" + os.sep
    if rel.startswith(prefix):
        rel = rel[len(prefix):]

    parts = rel.split(os.sep)[:-1]
    return os.sep.join(parts)


# -------------------------------------------------------------------------------
# 既存の内容の引き継ぎ
# -------------------------------------------------------------------------------
def existing_metadata(text, tag):
    """<ClCompile Include="..."> ... </ClCompile> の中身を拾う。

    Pch.cpp の PrecompiledHeader 設定のような、ファイル固有の指定を失わないため。
    """
    result = {}
    pattern = re.compile(r'<%s Include="([^"]+)"\s*>(.*?)</%s>' % (tag, tag), re.S)

    for m in pattern.finditer(text):
        body = m.group(2)
        if body.strip():
            result[m.group(1)] = body
    return result


def existing_guids(text):
    """既存のフィルタ GUID を引き継ぐ。"""
    result = {}
    pattern = re.compile(
        r'<Filter Include="([^"]+)">\s*<UniqueIdentifier>\{([^}]+)\}</UniqueIdentifier>', re.S)

    for m in pattern.finditer(text):
        result[m.group(1)] = m.group(2)
    return result


def replace_item_group(text, tag, new_body):
    """指定タグだけを含む ItemGroup を、新しい中身で置き換える。

    複数のタグが混在する ItemGroup（SlangTask + None など）は対象外にして守る。
    """
    pattern = re.compile(r'[ \t]*<ItemGroup>\s*(<%s Include=.*?)</ItemGroup>\r?\n' % tag, re.S)

    for m in pattern.finditer(text):
        others = re.findall(r'<(\w+) Include=', m.group(1))
        if any(o != tag for o in others):
            continue

        block = "  <ItemGroup>\r\n" + new_body + "  </ItemGroup>\r\n"
        return text[:m.start()] + block + text[m.end():]

    raise RuntimeError("ItemGroup for %s not found" % tag)


# -------------------------------------------------------------------------------
# 生成
# -------------------------------------------------------------------------------
def build_proj(text, sources, headers):
    meta_c = existing_metadata(text, "ClCompile")
    meta_i = existing_metadata(text, "ClInclude")

    def emit(tag, paths, meta):
        out = []
        for p in paths:
            if p in meta:
                out.append('    <%s Include="%s">%s</%s>\r\n' % (tag, p, meta[p], tag))
            else:
                out.append('    <%s Include="%s" />\r\n' % (tag, p))
        return "".join(out)

    text = replace_item_group(text, "ClInclude", emit("ClInclude", headers, meta_i))
    text = replace_item_group(text, "ClCompile", emit("ClCompile", sources, meta_c))
    return text


def emit_filter_entries(tag, paths):
    """.filters 用の項目を組み立てる。"""
    out = []
    for p in paths:
        f = filter_of(p)
        if f:
            out.append('    <%s Include="%s">\r\n'
                       '      <Filter>%s</Filter>\r\n'
                       '    </%s>\r\n' % (tag, p, f, tag))
        else:
            # ルート直下のファイルはフィルタを持たない
            out.append('    <%s Include="%s" />\r\n' % (tag, p))
    return "".join(out)


def build_filters(text, tag_to_paths):
    """.filters を、すべての項目種別ぶんまとめて作り直す。"""
    guids = existing_guids(text)

    # 必要なフィルタを、途中の階層も含めて洗い出す
    needed = set()
    for paths in tag_to_paths.values():
        for p in paths:
            f = filter_of(p)
            if not f:
                continue
            parts = f.split(os.sep)
            for i in range(1, len(parts) + 1):
                needed.add(os.sep.join(parts[:i]))

    filter_body = ""
    for name in sorted(needed, key=str.lower):
        guid = guids.get(name) or stable_guid(name)
        filter_body += ('    <Filter Include="%s">\r\n'
                        '      <UniqueIdentifier>{%s}</UniqueIdentifier>\r\n'
                        '    </Filter>\r\n' % (name, guid))

    for tag, paths in tag_to_paths.items():
        if not paths:
            continue
        text = replace_item_group(text, tag, emit_filter_entries(tag, paths))

    text = replace_item_group(text, "Filter", filter_body)
    return text


# -------------------------------------------------------------------------------
# エントリポイント
# -------------------------------------------------------------------------------
def main():
    check_only = "--check" in sys.argv

    sources, headers = collect_cpp_files()

    proj_before = read(PROJ)
    filt_before = read(FILTERS)

    # シェーダー等は .vcxproj の一覧をそのまま使う
    kept = {}
    for tag in KEPT_TAGS:
        paths = collect_includes(proj_before, tag)
        if paths:
            kept[tag] = paths

    proj_after = build_proj(proj_before, sources, headers)

    tag_to_paths = {"ClInclude": headers, "ClCompile": sources}
    tag_to_paths.update(kept)

    filt_after = build_filters(filt_before, tag_to_paths)

    changed = (proj_after != proj_before) or (filt_after != filt_before)

    print("C++ : sources %d / headers %d" % (len(sources), len(headers)))
    for tag, paths in kept.items():
        print("%s : %d" % (tag, len(paths)))

    # -------------------------------------------------------------------------------
    # 安全弁
    #
    # 走査の条件を間違えると、ビルドに必要なファイルが黙って project から消える。
    # 「消える」ものだけは必ず目に見える形で知らせる。
    # -------------------------------------------------------------------------------
    before_set = set(re.findall(r'<(?:ClInclude|ClCompile) Include="([^"]+)"', proj_before))
    after_set = set(sources) | set(headers)

    for p in sorted(after_set - before_set):
        print("  + %s" % p)
    for p in sorted(before_set - after_set):
        print("  - %s  <- プロジェクトから外れます" % p)

    if not changed:
        print("差分なし")
        return 0

    if check_only:
        print("差分あり（--check のため書き込みませんでした）")
        return 1

    write(PROJ, proj_after)
    write(FILTERS, filt_after)
    print("vcxproj と filters を更新しました")
    return 0


if __name__ == "__main__":
    sys.exit(main())
