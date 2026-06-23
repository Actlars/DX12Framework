/*
* struct TextureResource
* {
*     ComPtr<ID3D12Resource>      pResource;
*     D3D12_CPU_DESCRIPTOR_HANDLE HandleCPU = {};
*     D3D12_GPU_DESCRIPTOR_HANDLE HandleGPU = {};
*     uint32_t                    Width     = 0;
*     uint32_t                    Height    = 0;
*     DXGI_FORMAT                 Format    = DXGI_FORMAT_UNKNOWN;
* };
* 
* CPU/GPUハンドルの二重構造について
* DX12はCPUとGPUが別々のアドレス空間を持っている
* CPU側のプログラム → CPUハンドルでSRVを作成・書き込む
* GPU側のプログラム → GPUハンドルでSRVを参照する
* 
* DXGI_FORMATについて
* ピクセル１つのデータフォーマットを表す
* DXGI_FORMAT_R8G8B8A8_UNORM		一般的なPNG等
* DXGI_FORMAT_BC1_UNORM				DDS BC1圧縮。1/8サイズになる
* DXGI_FORMAT_R16G16B16A16_FLOAT	HDRテクスチャ。
* GPUはフォーマット情報をもとにメモリをどう読むかを決める
*/

/*
* struct InitDesc
* {
*     ComPtr<ID3D12Device>         pDevice;
*     ComPtr<ID3D12CommandQueue>   pQueue;
*     ComPtr<ID3D12DescriptorHeap> pHeapSRV;
*     uint32_t                     BaseSlot;
*     uint32_t                     MaxTextures;
* };
* 
* なぜ生ポインタではなくComPtrで受け取るのか
* ComPtrは参照カウンタ付きスマートポインタ。
* InitDescがコピーされたときに、参照カウントが増えるため
* 呼び出し元が解放されても、TextureManager内では安全に使い続けることができる
* 
* pQueueが必要な理由
* テクスチャのロードはファイル読み込みではなく、CPUメモリ → GPUメモリへのコピー命令が必要
* DX12のGPUへの命令はすべてCommandQueueを通さないと実行できないため、ここで受け取っている
* 
* pHeapSRVとBaseSlotの関係
* DescriptorHeapはSRVを並べる棚（配列）
*/

/*
* Init()の処理
* m_IncrSize = m_pDevice->GetDescriptorHandleIncrementSize(
*     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
* 棚の1マスのサイズをGPUから取得
* GPUメーカーによって値が違うため、ハードコードするとGPUが変わると壊れるので、この関数で取得
* 
*/

/*
* Load()の処理
* Load()が呼ばれる
*	|
*		キャッシュヒットチェック → 即座にスロット番号を返す（ロードしない）
*	|
*		スロット上限チェック
*	|
*		ファイル存在確認
*	|
*		DDSか？
*			YES → LoadFromDDSFile() ミップ・圧縮そのまま読む
*			NO  → LoadFromWICFile() + GenerateMipMaps()
*	|
*		UploadToGPU()
*	|
*		CreateSRV()
*	|
*		キャッシュ登録 → スロット番号を返す
* 
* 
* WICについて
* PNGやJPGはCPU向けの圧縮形式で、GPUが直接読むことができない
* WICででコードしてRGBA生データにした後、さらにミップマップを手動生成する必要がある
* 
* GenerateMipMapsの第3引数が０の理由
* DirectX::GenerateMipMaps(
    *scratchImage.GetImage(0, 0, 0),
    DirectX::TEX_FILTER_DEFAULT,
    0,   // ← これ
    mipped);

    0を渡すと「フルミップチェーンを自動計算して全部作る」という意味になる
    1024*1024のテクスチャなら512*512,256*256...1*1まで自動生成される
    ミップマップがないと遠距離のオブジェクトがちらつく（エイリアシング）ので
    WICファイルには必ず生成する
*/

/*
* UploadToGPU()の処理
* CPUメモリ                                GPUメモリ
*                      コピー                  コピー
* ファイルの生データ     →       UPLOADヒープ   →   DEFAULTヒープ
*                                   CPUが書ける         GPUが高速に読める
* 
* UPLOADヒープはCPUとGPUが療法アクセスできる領域だが、GPUからの読み取りが遅い
* DEFAULTヒープはGPUのみがアクセスでき、読み取りが高速
* テクスチャは毎フレーム読まれるので最終的にDEFAULTに置く必要がある
* 
* GetRequiredIntermediateSizeとは
* const UINT64 uploadSize = GetRequiredIntermediateSize(
    _outResource.Get(), 0, subresourceCount);
* UPLOADヒープのバッファをいくつのサイズで作ればよいかをDX12に計算させている
* ミップレベルや配列サイズによって必要なサイズが変わるため自分で計算せずAPIに任せる
* 
* サブリソースとは
* テクスチャ1枚の論理的な構成要素
* 通常のTexture2D（ミップ3段）
* サブリソース[0] : 1024*1024 （ミップ0）
* サブリソース[1] : 512*512   （ミップ1）
* サブリソース[2] : 256*256   （ミップ2）
* 
* Texture2DArray（2枚 ミップ2段）
* サブリソース[0] : 配列0 ミップ0
* サブリソース[1] : 配列0 ミップ1
* サブリソース[2] : 配列1 ミップ0
* サブリソース[3] : 配列1 ミップ1
* UpdateSubresources()はこれを全部まとめてUPLOAD → DEFAULTにコピーするd3dx12.hのヘルパー関数
* 
* リソースバリアとは
*   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
* DX12ではリソースが今何に使われているかを明示的に宣言する必要がある
* GPUは内部でキャッシュを持っており、用途が変わるタイミングでキャッシュのフラッシュや動機が必要
* バリアを忘れると描画結果が黒になったり古いデータが残ったりする
* 
* フェンスによる同期待ち
*   m_pQueue->Signal(pFence.Get(), 1);
    pFence->SetEventOnCompletion(1, hEvent);
    WaitForSingleObject(hEvent, INFINITE);
* ExecuteCommandLists()はGPUへの命令を予約するだけで、CPUはすぐ次の行に進む
* UploadBufferをComPtrで管理していてスコープアウト時に解放されるが
* GPUがコピーを終える前に解放されると壊れる
* そのためフェンスで「GPUがコピーを終えるまでCPUを止める」ことで安全を保証する
*/

/*
* CreateSRV()の処理
* 
* SRVとは何か
* Shader Resource Viewのこと。
* このGPUリソースをシェーダーからこのフォーマットで読んでくださいという説明書き
* 同じID3D12Resourceでも複数のSRVを作って別々のフォーマットで読むことができる
* DX12のビューという概念
* 
* 3種類のViewDimension
* if (_meta.IsCubemap())
    // キューブマップ: スカイボックス等。6面をまとめて1つのリソースとして扱う

else if (_meta.arraySize > 1)
    // 配列テクスチャ: 複数枚を1リソースにまとめたもの
    // AnimToTextureのBAT(Bone Animation Texture)がまさにこれ

else
    // 通常のTexture2D
* 
* これを間違えるとシェーダーが正しくサンプリングできない
* DirectXTexのメタデータから自動判別することで呼び出し側が意識しなくて済む
*/

/*
* D3D12_HEAP_PROPERTIESについて
* GPUリソースをどの種類のメモリに置くかを指定する構造体
* D3D12_HEAP_TYPE_DEFAULTはGPU専用メモリ（VRAM）を意味する - CPUからは直接書き込めないが、GPUからの読み取りが高速
* D3D12_HEAP_TYPE_UPLOAD : CPUとGPUの両方からアクセス可。CPUが書いてGPUに渡す中間バッファ用
* D3D12_HEAP_TYPE_READBACK : GPUが書いてCPUが読むよう。デバッグのreadback等
* 
*/