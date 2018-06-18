//
// inlined hlsl shaders.  see dxwin.h
//

const char *dxshaders = R"EFFECTFILE(

float4 qconj(float4 q)
{
	return float4(-q.x,-q.y,-q.z,q.w);
}

float4 qmul(float4 a, float4 b)
{
	float4 c;
	c.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z; 
	c.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y; 
	c.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x; 
	c.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w; 
	return c;
}


float3 qrot(  float4 q,  float3 v )
{
	return qmul(qmul(q,float4(v,0)), float4(-q.x,-q.y,-q.z,q.w)).xyz; 
}

struct Vertex 
{
    float3 position    : POSITION;
    float4 orientation : TEXCOORD1;
    float2 texcoord	   : TEXCOORD0;
};

struct Fragment
{
    float4 screenpos   : SV_POSITION;
    float2 texcoord    : TEXCOORD0;
	float3 position    : TEXCOORD6;
//	float4 orientation : TEXCOORD7; 
	float3 tangent     : TEXCOORD3; 
	float3 binormal    : TEXCOORD4; 
	float3 normal      : TEXCOORD5; 
};

cbuffer ConstantBuffer : register( b0 )
{
	float4 hack;
	matrix Projection;
	float3 camerap; float unusedc;  // extra float for 128bit padding
	float4 cameraq;
	float3 meshp;   float unusedm;
	float4 meshq;
}



Texture2D    txDiffuse : register( t0 );
Texture2D    txNMap    : register( t1 );
SamplerState samLinear : register( s0 );


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
//Fragment VS(Vertex v)
Fragment VS( Vertex v ) //: SV_POSITION
{
	Fragment foo = (Fragment)0;
	//out.position = meshp + qrot(meshq,v.position);
	//out.screenpos = mul(float4( mul(qrot(qconj(cameraq,out.position))-camerap,1),Projection);
	//	out.screenpos = mul(float4(v.position,1),Projection);
	//	return out.screenpos;
	float3 pw = meshp.xyz + qrot(meshq,v.position.xyz) ;
	foo.position = pw; // world space position
	float4 pc = float4(qrot(qconj(cameraq),pw-camerap) ,1);
	foo.screenpos =  mul(pc,Projection);
	foo.texcoord = v.texcoord;
	float4 sq = qmul(meshq,v.orientation);
	foo.normal   = qrot(sq,float3(0,0,1));
	foo.tangent  = qrot(sq,float3(1,0,0));
	foo.binormal = qrot(sq,float3(0,1,0));
	return foo;
	//   return mul(p,Projection);
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( Fragment v) : SV_Target
{
	float parallaxscale = 0.2;  // was 0.06

    // return hack /* txDiffuse.Sample( samLinear,v.texcoord) */ * txNMap.Sample( samLinear,v.texcoord).w  ; // float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
	float height = txNMap.Sample( samLinear,v.texcoord).w * parallaxscale - parallaxscale/2.0f;
	float3x3 m = float3x3(normalize(v.tangent),normalize(v.binormal),normalize(v.normal));
	float2 offset = height * mul(m,normalize(camerap-v.position)) ; // rotate eye vector into tangent space

	float3 nl = normalize(txNMap.Sample( samLinear,v.texcoord+offset).xyz - float3(0.5,0.5,0.5));
	float3 nw = mul(nl,m);
//return float4(nl*0.5+float3(0.5,0.5,0.5),1.0);
//    return hack * dot(nw ,qrot(cameraq,float3(0,0,1)) )  ; // float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
//    return dot(nw ,qrot(cameraq,float3(-.5,.5,.5)) )  ; // float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
//    return dot(nw ,normalize(float3(-1,-2,1)) )  ; // float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
//return txNMap.Sample( samLinear,v.texcoord).zzzz;
//return float4(0.5+offset.x*100,0.5+offset.y*100,parallaxscale*100,1);
  return hack * (0.75+0.25*txDiffuse.Sample( samLinear,v.texcoord+offset)) * dot(nw ,normalize(float3(-1,-2,1)) )  ; // float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
}

)EFFECTFILE";
