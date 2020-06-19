#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <d3dcompiler.h>
#include "D3DUtil.h"
#include "DXSampleHelper.h"

ComPtr<ID3D12Resource> D3DUtil::CreateDefaultBuffer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const void * data, size_t byteSize, ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    ThrowIfFailed(pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize), D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    ThrowIfFailed(pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    UINT8* pVertexDataBegin = nullptr;
    uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, data, byteSize);
    uploadBuffer->Unmap(0, nullptr);

    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    pCommandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    return defaultBuffer;
}

ComPtr<ID3DBlob> D3DUtil::CompileShader(const std::wstring & fileName, const D3D_SHADER_MACRO * defines, const std::string & entryPoint, const std::string & target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // DEBUG

    HRESULT hr = S_OK;
    ComPtr<ID3DBlob> byteCode;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(fileName.c_str(), defines, nullptr, entryPoint.c_str(), target.c_str(), compileFlags, 0, byteCode.GetAddressOf(), errors.GetAddressOf());

    if (errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return byteCode;
}

ComPtr<ID3DBlob> D3DUtil::LoadShaderBinary(const std::wstring & fileName)
{
    ComPtr<ID3DBlob> byteCode;
    std::ifstream stream(fileName, std::ios::binary);
    ThrowIfFailed(stream.is_open());
    stream.seekg(std::ios_base::end);
    size_t size = stream.tellg();
    stream.seekg(std::ios_base::beg);
    D3DCreateBlob(size, &byteCode);
    stream.read(reinterpret_cast<char*>(byteCode->GetBufferPointer()), size);
    stream.close();
    return byteCode;
}

UINT D3DUtil::CalcCBufferSize(UINT size)
{
    return (size + 255) & ~255;
}
