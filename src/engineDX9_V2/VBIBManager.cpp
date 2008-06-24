
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// STATIC STORAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////
CVBIBManager::SVBIBManagerState CVBIBManager::sVBIB = { false };



////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVBIBManager::Initialise( )
{
	if( sVBIB.bInitialised == false )
	{
		// Clear out the struct.
		memset( &sVBIB, 0, sizeof( CVBIBManager::SVBIBManagerState ) );

		// Flag that its now valid and ready to use.
		sVBIB.bInitialised = true;
	}
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Shutdown()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVBIBManager::Shutdown( )
{
	if( sVBIB.bInitialised == true )
	{
		DWORD i;
		ULONG refCount;
		for( i=0; i<sVBIB.dwNumBuffersAllocated; i++ )
		{
//			_ASSERT( sVBIB.pBufferArray[i] != NULL );

			if( sVBIB.pBufferArray[i] != NULL )
			{
				switch( sVBIB.pBufferArray[i]->eBufferType )
				{
				case eBT_Vertex:
				case eBT_VertexDynamic:
//					_ASSERT( sVBIB.pBufferArray[i]->pVertexBuffer != NULL );
					if( sVBIB.pBufferArray[i]->pVertexBuffer )
					{
						refCount = sVBIB.pBufferArray[i]->pVertexBuffer->Release();
						sVBIB.pBufferArray[i]->pVertexBuffer = NULL;
					}
					break;

				case eBT_Index:
//					_ASSERT( sVBIB.pBufferArray[i]->pIndexBuffer != NULL );
					if( sVBIB.pBufferArray[i]->pIndexBuffer )
					{
						refCount = sVBIB.pBufferArray[i]->pIndexBuffer->Release();
						sVBIB.pBufferArray[i]->pIndexBuffer = NULL;
					}
					break;

				default:
					_ASSERT( false );
				}

				delete [] sVBIB.pBufferArray[i];
				sVBIB.pBufferArray[i] = NULL;
			}
		}

		// Clear out the struct.
		sVBIB.bInitialised = false;
	}
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocateVertexRegion()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVBIBManager::EvictDefaultPoolResources( )
{
	DWORD i;

	for( i=0; i<sVBIB.dwNumBuffersAllocated; i++ )
	{
		switch( sVBIB.pBufferArray[i]->eBufferType )
		{
		case eBT_Vertex:
		case eBT_Index:
			// Nothing to be done here.
			break;

		case eBT_VertexDynamic:
			if( ( sVBIB.pBufferArray[i]->bDefaultPool == true ) &&
				( sVBIB.pBufferArray[i]->bResourceEvicted == false ) &&
				( sVBIB.pBufferArray[i]->pVertexBuffer != NULL ) )
			{
				sVBIB.pBufferArray[i]->pVertexBuffer->Release();
				sVBIB.pBufferArray[i]->pVertexBuffer = NULL;
				sVBIB.pBufferArray[i]->dwBufferOffset = 0;
				sVBIB.pBufferArray[i]->bResourceEvicted = true;
			}
			break;

		default:
			_ASSERT( false );		// Which buffer type was required?
		}
	}
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocateVertexRegion()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AllocateVertexRegion(	SVBIBHandle * pResult,
											DWORD dwNumVertices,
											DWORD dwVertexSize,
											DWORD dwFVF,
											void ** ppData )
{
	HRESULT hr;
	DWORD dwBufferIndex;
	SD3DBuffer * pBuffer;

	// Calls to AllocateVertexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticVertexBufferLocked == false );

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Vertex,
								dwFVF,
								dwVertexSize,
								dwNumVertices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= dwFVF;
		pBuffer->dwElementSize	= dwVertexSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwVertexSize;
		pBuffer->eBufferType	= eBT_Vertex;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					dwFVF,
					c_iDefaultBufferPool,
					&pBuffer->pVertexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pVertexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumVertices,
										ppData,
										0 );
	_ASSERT( hr == D3D_OK );
	_ASSERT( ppData != NULL );

	pBuffer->bLocked = true;
	sVBIB.bStaticVertexBufferLocked = true;

	// Update the result structure.
	pResult->eType					= eBT_Vertex;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= dwFVF;
	pResult->dwNumElements			= dwNumVertices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumVertices;
	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocateIndexRegion()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AllocateIndexRegion(	SVBIBHandle * pResult,
										DWORD dwNumIndices,
										D3DFORMAT d3dIndexFormat,
										void ** ppData )
{
	HRESULT hr;
	DWORD dwBufferIndex, dwIndexValueSize;
	SD3DBuffer * pBuffer;

	// Calls to AllocateIndexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticIndexBufferLocked == false );
	_ASSERT( ( d3dIndexFormat == D3DFMT_INDEX16 ) || ( d3dIndexFormat == D3DFMT_INDEX32 ) );

	dwIndexValueSize = (d3dIndexFormat == D3DFMT_INDEX16 ) ? 2 : 4;

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Index,
								d3dIndexFormat,
								dwIndexValueSize,
								dwNumIndices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= d3dIndexFormat;
		pBuffer->dwElementSize	= dwIndexValueSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwIndexValueSize;
		pBuffer->eBufferType	= eBT_Index;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateIndexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					d3dIndexFormat,
					c_iDefaultBufferPool,
					&pBuffer->pIndexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pIndexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumIndices,
										ppData,
										0 );
	_ASSERT( hr == D3D_OK );
	pBuffer->bLocked				= true;
	sVBIB.bStaticIndexBufferLocked	= true;

	// Update the result structure.
	pResult->eType					= eBT_Index;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= d3dIndexFormat;
	pResult->dwNumElements			= dwNumIndices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumIndices;
	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// ReleaseRegion()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::ReleaseRegion( SVBIBHandle * pHandle )
{
	HRESULT hr;

	_ASSERT( pHandle != NULL );
	SD3DBuffer * pBuffer = sVBIB.pBufferArray[ pHandle->dwBufferIndex ];

	switch( pBuffer->eBufferType )
	{
	case eBT_Vertex:
		_ASSERT( sVBIB.bStaticVertexBufferLocked == true );
		_ASSERT( pBuffer->bLocked == true );
		hr = pBuffer->pVertexBuffer->Unlock();
		_ASSERT( hr == D3D_OK );
		break;

	case eBT_Index:
		_ASSERT( sVBIB.bStaticVertexBufferLocked == true );
		_ASSERT( pBuffer->bLocked == true );
		hr = pBuffer->pIndexBuffer->Unlock();
		_ASSERT( hr == D3D_OK );
		break;

	case eBT_VertexDynamic:
	default:
		// ReleaseRegion called on invalid buffer.
		_ASSERT( false );
		return false;
	}
	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AddVerticesToBuffer()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AddVerticesToBuffer(	SVBIBHandle * pResult,
										BYTE * pVertices,
										DWORD dwNumVertices,
										DWORD dwVertexSize,
										DWORD dwFVF )
{
	HRESULT hr;
	DWORD dwBufferIndex;
	SD3DBuffer * pBuffer;
	BYTE * pData;

	// Calls to AllocateVertexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticVertexBufferLocked == false );

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Vertex,
								dwFVF,
								dwVertexSize,
								dwNumVertices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= dwFVF;
		pBuffer->dwElementSize	= dwVertexSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwVertexSize;
		pBuffer->eBufferType	= eBT_Vertex;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					dwFVF,
					c_iDefaultBufferPool,
					&pBuffer->pVertexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pVertexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumVertices,
										(void**) &pData,
										0 );
	_ASSERT( hr == D3D_OK );

	memcpy( pData, pVertices, pBuffer->dwElementSize * dwNumVertices );

	hr = pBuffer->pVertexBuffer->Unlock();
	_ASSERT( hr == D3D_OK );

	// Update the result structure.
	pResult->eType					= eBT_Vertex;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= dwFVF;
	pResult->dwNumElements			= dwNumVertices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumVertices;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AddIndicesToBuffer()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AddIndicesToBuffer(	SVBIBHandle * pResult,
										BYTE * pIndices,
										DWORD dwNumIndices,
										D3DFORMAT d3dIndexFormat )
{
	HRESULT hr;
	DWORD dwBufferIndex, dwIndexValueSize;
	SD3DBuffer * pBuffer;
	BYTE * pData;

	// Calls to AllocateIndexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticIndexBufferLocked == false );
	_ASSERT( ( d3dIndexFormat == D3DFMT_INDEX16 ) || ( d3dIndexFormat == D3DFMT_INDEX32 ) );

	dwIndexValueSize = (d3dIndexFormat == D3DFMT_INDEX16 ) ? 2 : 4;

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Index,
								d3dIndexFormat,
								dwIndexValueSize,
								dwNumIndices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= d3dIndexFormat;
		pBuffer->dwElementSize	= dwIndexValueSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwIndexValueSize;
		pBuffer->eBufferType	= eBT_Index;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateIndexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					d3dIndexFormat,
					c_iDefaultBufferPool,
					&pBuffer->pIndexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pIndexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumIndices,
										(void**) &pData,
										0 );
	_ASSERT( hr == D3D_OK );

	memcpy( pData, pIndices, pBuffer->dwElementSize * dwNumIndices );

	hr = pBuffer->pIndexBuffer->Unlock();
	_ASSERT( hr == D3D_OK );

	// Update the result structure.
	pResult->eType					= eBT_Index;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= d3dIndexFormat;
	pResult->dwNumElements			= dwNumIndices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumIndices;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// FindBuffer()
// Given parameters find a matching buffer if one exists.
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CVBIBManager::FindBuffer( EBufferType eBufferType, 
								DWORD dwFormat,				// Either FVF or index format. 
								DWORD dwElementSize,
								DWORD dwNumElements )
{
	DWORD i;
	bool bBufferFound;

	_ASSERT((eBufferType > eBT_Invalid) && (eBufferType < eBT_NumTypes) );
	bBufferFound = false;

	for( i=0; i<sVBIB.dwNumBuffersAllocated; i++ )
	{
		switch( eBufferType )
		{
		case eBT_Vertex:
		case eBT_Index:
			if( ( sVBIB.pBufferArray[i]->eBufferType == eBufferType ) &&
				( sVBIB.pBufferArray[i]->dwBufferFormat == dwFormat ) &&
				( sVBIB.pBufferArray[i]->dwElementSize == dwElementSize ) &&
				( sVBIB.pBufferArray[i]->dwBufferOffset + 
						( dwNumElements * dwElementSize ) <= sVBIB.pBufferArray[i]->dwBufferSize ) )
			{
				bBufferFound = true;
			}
			break;
		case eBT_VertexDynamic:
			if( ( sVBIB.pBufferArray[i]->eBufferType == eBufferType ) &&
				( sVBIB.pBufferArray[i]->dwBufferFormat == dwFormat ) &&
				( sVBIB.pBufferArray[i]->dwElementSize == dwElementSize ) &&
				( sVBIB.pBufferArray[i]->dwBufferSize <= dwNumElements * dwElementSize ) )
			{
				bBufferFound = true;
			}
			break;

		default:
			_ASSERT( false );		// Which buffer type was required?
		}

		if( bBufferFound == true )
		{
			break;
		}
	}

	if( bBufferFound == false )
	{
		return 0xFFFFFFFF;
	}
	return i;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// SetVertexAndIndexStreams()
// Set the vertex and index streams.
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CVBIBManager::SetVertexAndIndexStreams(	const SVBIBHandle * pVB,
												const SVBIBHandle * pIB )
{
	HRESULT hr;
	hr = CD3DDevice9::SetStreamSource(
			0, 
			sVBIB.pBufferArray[ pVB->dwBufferIndex ]->pVertexBuffer,
			pVB->dwFirstElementOffset,
			sVBIB.pBufferArray[ pVB->dwBufferIndex ]->dwElementSize );
	_ASSERT( hr == S_OK );

	hr = CD3DDevice9::SetIndices( sVBIB.pBufferArray[ pIB->dwBufferIndex ]->pIndexBuffer );
	_ASSERT( hr == S_OK );

	return hr;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// SetVertexStream()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CVBIBManager::SetVertexStream( const SVBIBHandle * pVB )
{
	HRESULT hr;
	if( pVB == NULL )
	{
		hr = CD3DDevice9::SetStreamSource( 0, NULL, 0, 0 );
	}
	else
	{
		hr = CD3DDevice9::SetStreamSource(
				0, 
				sVBIB.pBufferArray[ pVB->dwBufferIndex ]->pVertexBuffer,
				0,
				sVBIB.pBufferArray[ pVB->dwBufferIndex ]->dwElementSize );
	}
	_ASSERT( hr == S_OK );

	return hr;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// SetVertexStream()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CVBIBManager::SetIndexStream( const SVBIBHandle * pIB )
{
	HRESULT hr;
	if( pIB == NULL )
	{
		hr = CD3DDevice9::SetIndices( NULL );
	}
	else
	{
		hr = CD3DDevice9::SetIndices( sVBIB.pBufferArray[ pIB->dwBufferIndex ]->pIndexBuffer );
	}
	_ASSERT( hr == S_OK );

	return hr;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AddLegacyVerticesToBuffer()
// Separate function for reordering vertex data to match FFP layout.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AddLegacyVerticesToBuffer(	SVBIBHandle * pResult,
												BYTE * pVertices,
												DWORD dwNumVertices,
												DWORD dwVertexSize,
												DWORD dwFVF )
{
	HRESULT hr;
	DWORD dwBufferIndex;
	SD3DBuffer * pBuffer;
	BYTE * pData;

	// Calls to AllocateVertexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticVertexBufferLocked == false );
	_ASSERT( dwFVF != 0 );

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Vertex,
								dwFVF,
								dwVertexSize,
								dwNumVertices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= dwFVF;
		pBuffer->dwElementSize	= dwVertexSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwVertexSize;
		pBuffer->eBufferType	= eBT_Vertex;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					dwFVF,
					c_iDefaultBufferPool,
					&pBuffer->pVertexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pVertexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumVertices,
										(void**) &pData,
										0 );
	_ASSERT( hr == D3D_OK );

	// Reorder the data.
	DWORD i;
	Vertex * pSrcData = (Vertex *) pVertices;
	D3DVERTEX d3dVertex; 
	for( i=0; i<dwNumVertices; i++ )
	{
		d3dVertex.x = pSrcData[i].x;
		d3dVertex.y = pSrcData[i].y;
		d3dVertex.z = pSrcData[i].z;
		d3dVertex.nx = pSrcData[i].nx;
		d3dVertex.ny = pSrcData[i].ny;
		d3dVertex.nz = pSrcData[i].nz;
		d3dVertex.tu = pSrcData[i].u;
		d3dVertex.tv = pSrcData[i].v;

		memcpy( pData, &d3dVertex, sizeof(D3DVERTEX) );
		pData += sizeof(D3DVERTEX);
	}

	hr = pBuffer->pVertexBuffer->Unlock();
	_ASSERT( hr == D3D_OK );

	// Update the result structure.
	pResult->eType					= eBT_Vertex;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= dwFVF;
	pResult->dwNumElements			= dwNumVertices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumVertices;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AddLegacyLitVerticesToBuffer()
// Separate function for reordering vertex data to match FFP layout.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AddLegacyLitVerticesToBuffer(	SVBIBHandle * pResult,
													BYTE * pVertices,
													DWORD dwNumVertices,
													DWORD dwVertexSize,
													DWORD dwFVF )
{
	HRESULT hr;
	DWORD dwBufferIndex, dwFVFVertexSize;
	SD3DBuffer * pBuffer;
	BYTE * pData;

	// Calls to AllocateVertexRegion() must be accompanied by a ReleaseRegion() call.
	_ASSERT( sVBIB.bStaticVertexBufferLocked == false );
	_ASSERT( dwFVF != 0 );
	dwFVFVertexSize = sizeof( D3DLVERTEX );

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_Vertex,
								dwFVF,
								dwFVFVertexSize,
								dwNumVertices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= dwFVF;
		pBuffer->dwElementSize	= dwFVFVertexSize;
		pBuffer->dwBufferSize	= c_iDefaultBufferSize * dwFVFVertexSize;
		pBuffer->eBufferType	= eBT_Vertex;
		pBuffer->dwBufferOffset = 0;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					c_iDefaultBufferSize * pBuffer->dwElementSize,
					D3DUSAGE_WRITEONLY,
					dwFVF,
					c_iDefaultBufferPool,
					&pBuffer->pVertexBuffer,
					NULL );

		_ASSERT( hr == D3D_OK );
	}

	// Lock the buffer and copy the data in.
	hr = pBuffer->pVertexBuffer->Lock(	pBuffer->dwBufferOffset,
										pBuffer->dwElementSize * dwNumVertices,
										(void**) &pData,
										0 );
	_ASSERT( hr == D3D_OK );

	// Reorder the data.
	DWORD i;
	VertexL * pSrcData = (VertexL *) pVertices;
	D3DLVERTEX litVertex; 
	for( i=0; i<dwNumVertices; i++ )
	{
		litVertex.x = pSrcData[i].x;
		litVertex.y = pSrcData[i].y;
		litVertex.z = pSrcData[i].z;
		litVertex.dcSpecular = 0;
		litVertex.dwReserved = 0;
		litVertex.dcColor = D3DCOLOR_COLORVALUE( pSrcData[i].r, pSrcData[i].g, pSrcData[i].b, pSrcData[i].a );
		litVertex.tu = pSrcData[i].u;
		litVertex.tv = pSrcData[i].v;

		memcpy( pData, &litVertex, sizeof(D3DLVERTEX) );
		pData += sizeof(D3DLVERTEX);
	}

	hr = pBuffer->pVertexBuffer->Unlock();
	_ASSERT( hr == D3D_OK );

	// Update the result structure.
	pResult->eType					= eBT_Vertex;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= dwFVF;
	pResult->dwNumElements			= dwNumVertices;
	pResult->dwFirstElementOffset	= pBuffer->dwBufferOffset;

	// Update the buffer offset.
	pBuffer->dwBufferOffset += pBuffer->dwElementSize * dwNumVertices;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocateDynamicVertexBuffer()
// Acquire a buffer for adding dynamic vertices. An attempt is made to locate an
// existing dynamic vertex buffer, large enough to support the max vertices passed
// in. The corresponding LockDynamicVertexBuffer() function must be called to add
// vertex data to the stream.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::AllocateDynamicVertexBuffer(	SVBIBHandle * pResult,
												DWORD dwMaxVertices,
												DWORD dwVertexSize,
												DWORD dwFVF )
{
	HRESULT hr;
	DWORD dwBufferIndex;
	SD3DBuffer * pBuffer;

	// Attempt to locate an existing buffer.
	dwBufferIndex = FindBuffer( eBT_VertexDynamic,
								dwFVF,
								dwVertexSize,
								dwMaxVertices );

	if( dwBufferIndex != 0xFFFFFFFF )
	{
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];
	}
	else
	{
		// Create a new buffer.
		dwBufferIndex = sVBIB.dwNumBuffersAllocated;
		sVBIB.dwNumBuffersAllocated ++;
		
		sVBIB.pBufferArray[dwBufferIndex] = new SD3DBuffer;
		memset( sVBIB.pBufferArray[dwBufferIndex], 0, sizeof( SD3DBuffer ) );
		pBuffer = sVBIB.pBufferArray[dwBufferIndex];

		// Configure the new buffer.
		pBuffer->dwBufferFormat	= dwFVF;
		pBuffer->dwElementSize	= dwVertexSize;
		pBuffer->dwBufferSize	= dwMaxVertices * dwVertexSize;
		pBuffer->eBufferType	= eBT_VertexDynamic;
		pBuffer->dwBufferOffset = 0;
		pBuffer->bLocked		= false;
		pBuffer->bDefaultPool	= ( c_iDynamicBufferPool == D3DPOOL_DEFAULT ) ? true : false;

		// Create the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					dwMaxVertices * pBuffer->dwElementSize,
					D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
					dwFVF,
					c_iDynamicBufferPool,
					&pBuffer->pVertexBuffer,
					NULL );
		_ASSERT( hr == D3D_OK );
	}

	// Update the result structure.
	pResult->eType					= eBT_VertexDynamic;
	pResult->dwBufferIndex			= dwBufferIndex;
	pResult->dwBufferFormat			= dwFVF;
	pResult->dwNumElements			= dwMaxVertices;
	pResult->dwFirstElementOffset	= 0;

	// Reset the buffer offset.
	pBuffer->dwBufferOffset = 0;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// LockDynamicVertexBuffer()
// Store the offset in the handle.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::LockDynamicVertexBuffer(	SVBIBHandle * pHandle,
											DWORD dwNumVertsRequired,	
											void ** ppbData )
{
	HRESULT hr;
	DWORD dwLockFlags = D3DLOCK_NOOVERWRITE;
	DWORD dwSizeToLock;
	SD3DBuffer * pVB = sVBIB.pBufferArray[ pHandle->dwBufferIndex ];

	_ASSERT( pVB->eBufferType == eBT_VertexDynamic );
	_ASSERT( pVB->bLocked == false );

	if( pVB->bResourceEvicted == true )
	{
		_ASSERT( pVB->pVertexBuffer == NULL );

		// Recreate the resource.
		hr = CD3DDevice9::Device()->CreateVertexBuffer(
					pVB->dwBufferSize,
					D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
					pVB->dwBufferFormat,
					c_iDynamicBufferPool,
					&pVB->pVertexBuffer,
					NULL );
		_ASSERT( hr == D3D_OK );
		pVB->bResourceEvicted = false;
	}
	_ASSERT( pVB->pVertexBuffer != NULL );

	dwSizeToLock = dwNumVertsRequired * pVB->dwElementSize;
	if( ( pVB->dwBufferOffset * pVB->dwElementSize ) > ( pVB->dwBufferSize - dwSizeToLock ) )
	{
		// Lock discard.
		dwLockFlags = D3DLOCK_DISCARD;
		pVB->dwBufferOffset = 0;
	}

	hr = pVB->pVertexBuffer->Lock(	pVB->dwBufferOffset * pVB->dwElementSize,
									dwSizeToLock,
									ppbData,
									dwLockFlags );
	_ASSERT( hr == D3D_OK );
	if( hr != D3D_OK )
	{
		return false;
	}

	pHandle->dwFirstElementOffset = pVB->dwBufferOffset;
	pVB->bLocked = true;
	pVB->dwBufferOffset += dwNumVertsRequired;
	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// UnlockDynamicVertexBuffer()
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVBIBManager::UnlockDynamicVertexBuffer( SVBIBHandle * pHandle )
{
	HRESULT hr;
	SD3DBuffer * pVB = sVBIB.pBufferArray[ pHandle->dwBufferIndex ];

	_ASSERT( pVB->pVertexBuffer != NULL );
	_ASSERT( pVB->bLocked == true );

	hr = pVB->pVertexBuffer->Unlock( );
	_ASSERT( hr == D3D_OK );

	pVB->bLocked = false;
	return true;
}

