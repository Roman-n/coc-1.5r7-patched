#ifndef QueryHelper_included
#define QueryHelper_included
#pragma once

//	Interface
IC HRESULT CreateQuery(ID3DQuery** ppQuery);
IC HRESULT GetData(ID3DQuery* pQuery, void* pData, UINT DataSize);
IC HRESULT BeginQuery(ID3DQuery* pQuery);
IC HRESULT EndQuery(ID3DQuery* pQuery);
IC HRESULT ReleaseQuery(ID3DQuery *pQuery);

//	Implementation

#if defined(USE_DX11)

IC HRESULT CreateQuery(ID3DQuery** ppQuery)
{
    D3D_QUERY_DESC desc;
    desc.MiscFlags = 0;
    desc.Query = D3D_QUERY_OCCLUSION;
    return HW.pDevice->CreateQuery(&desc, ppQuery);
}

IC HRESULT GetData(ID3DQuery* pQuery, void* pData, UINT DataSize)
{
    //	Use D3Dxx_ASYNC_GETDATA_DONOTFLUSH for prevent flushing
    return HW.pContext->GetData(pQuery, pData, DataSize, 0);
}

IC HRESULT BeginQuery(ID3DQuery* pQuery)
{
    HW.pContext->Begin(pQuery);
    return S_OK;
}

IC HRESULT EndQuery(ID3DQuery* pQuery)
{
    HW.pContext->End(pQuery);
    return S_OK;
}

IC HRESULT ReleaseQuery(ID3DQuery* pQuery)
{
    _RELEASE(pQuery);
    return S_OK;
}

#elif defined(USE_DX10)

IC HRESULT CreateQuery(ID3DQuery** ppQuery)
{
    D3D_QUERY_DESC desc;
    desc.MiscFlags = 0;
    desc.Query = D3D_QUERY_OCCLUSION;
    return HW.pDevice->CreateQuery(&desc, ppQuery);
}

IC HRESULT GetData(ID3DQuery* pQuery, void* pData, UINT DataSize)
{
    //	Use D3Dxx_ASYNC_GETDATA_DONOTFLUSH for prevent flushing
    return pQuery->GetData(pData, DataSize, 0);
}

IC HRESULT BeginQuery(ID3DQuery* pQuery)
{
    pQuery->Begin();
    return S_OK;
}

IC HRESULT EndQuery(ID3DQuery* pQuery)
{
    pQuery->End();
    return S_OK;
}

IC HRESULT ReleaseQuery(ID3DQuery* pQuery)
{
    _RELEASE(pQuery);
    return S_OK;
}

#else //	USE_DX10

IC HRESULT CreateQuery(ID3DQuery** ppQuery) { return HW.pDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, ppQuery); }
IC HRESULT GetData(ID3DQuery* pQuery, void* pData, UINT DataSize)
{
    return pQuery->GetData(pData, DataSize, D3DGETDATA_FLUSH);
}

IC HRESULT BeginQuery(ID3DQuery* pQuery) { return pQuery->Issue(D3DISSUE_BEGIN); }
IC HRESULT EndQuery(ID3DQuery* pQuery) { return pQuery->Issue(D3DISSUE_END); }

IC HRESULT ReleaseQuery(ID3DQuery* pQuery)
{
    _RELEASE(pQuery);
    return S_OK;
}

#endif //	USE_DX10

#endif // QueryHelper_included
