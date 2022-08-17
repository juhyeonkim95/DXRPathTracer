#include "DX12StateSubobjects.h"

ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ID3DBlobPtr pSigBlob;
    ID3DBlobPtr pErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
        msgBox(msg);
        return nullptr;
    }
    ID3D12RootSignaturePtr pRootSig;
    d3d_call(pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig)));
    return pRootSig;
}
