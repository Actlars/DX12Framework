// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "TextureManager.h"

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool TextureManager::Init(const InitDesc& _desc)
{
    // 引数チェック
    assert(_desc.pDevice != nullptr);
    assert(_desc.pQueue != nullptr);
    assert(_desc.pPool != nullptr);
    assert(_desc.MaxTextures > 0);

    // 参照を保持（所有権は持たない）
    m_pDevice = _desc.pDevice;
    m_pQueue = _desc.pQueue;
    m_pPool = _desc.pPool;
    m_MaxTextures = _desc.MaxTextures;
    m_NextSlot = 0;

    // スロット配列を事前確保
    // resize: Load()内でm_Textures[slot]に添字アクセスするため要素が必要
    m_Textures.resize(m_MaxTextures);

    // reserve: Load()のたびに再確保が起きないようにハッシュマップのバケットを確保
    m_PathToSlot.reserve(m_MaxTextures);

    return true;
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void TextureManager::Term()
{
    // unique_ptr なので clear() するだけで全 Texture の Term() が呼ばれる
    // Texture::Term() 内で DescriptorPool へのハンドル返却も行われる
    m_Textures.clear();
    m_PathToSlot.clear();
    m_NextSlot = 0;

    m_pDevice = nullptr;
    m_pQueue = nullptr;
    m_pPool = nullptr;
}

// -------------------------------------------------------------------------------
// テクスチャのロード
// -------------------------------------------------------------------------------
bool TextureManager::Load(const std::wstring& _path, uint32_t& _outSlot)
{
    // キャッシュヒット: 同一パスが既にロード済みならGPUアップロードを省略
    if (IsLoaded(_path))
    {
        _outSlot = m_PathToSlot.at(_path);
        return true;
    }

    // スロット上限チェック
    if (m_NextSlot >= m_MaxTextures)
    {
        assert(false && "TextureManager: スロット上限に達しました");
        return false;
    }

    // Texture を生成してロード
    // unique_ptr で所有権を明確化し、エラー時の解放漏れを防ぐ
    auto tex = std::make_unique<Texture>();

    if (!tex->Init(m_pDevice, m_pQueue, m_pPool, _path))
    {
        // ロード失敗: tex は unique_ptr のスコープアウトで自動解放される
        return false;
    }

    // スロットに登録
    // 全処理が成功した最後にのみスロットを消費する
    // 途中で失敗してもスロットが無駄に消費されない
    const uint32_t slot = m_NextSlot;
    m_Textures[slot] = std::move(tex);
    m_PathToSlot[_path] = slot;
    _outSlot = slot;
    ++m_NextSlot;

    return true;
}

// -------------------------------------------------------------------------------
// スロット番号でテクスチャを取得
// -------------------------------------------------------------------------------
const Texture* TextureManager::GetBySlot(uint32_t _slot) const
{
    if (_slot >= m_NextSlot)
    {
        return nullptr;
    }

    return m_Textures[_slot].get();
}

// -------------------------------------------------------------------------------
// パスでテクスチャを取得
// -------------------------------------------------------------------------------
const Texture* TextureManager::GetByPath(const std::wstring& _path) const
{
    auto it = m_PathToSlot.find(_path);
    if (it == m_PathToSlot.end())
    {
        return nullptr;
    }

    return m_Textures[it->second].get();
}

// -------------------------------------------------------------------------------
// ロード済み確認
// -------------------------------------------------------------------------------
bool TextureManager::IsLoaded(const std::wstring& _path) const
{
    return m_PathToSlot.count(_path) > 0;
}