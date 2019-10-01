//------------
//  (c) Stan Melax 2006-2008
//  part of a collection of geometry code.
//  class Face to represent,split,crop,etc, single polygons (not as mesh or winged edge)
//  used for surfaces/breps info.  texture coords are stored as a linear transform of position. 
//

#include <math.h>
#include <assert.h>

#include "bsp.h"

template<class T> inline T Pop(std::vector<T> & c) { auto val = std::move(c.back()); c.pop_back(); return val; }

// where is pi supposed to be?
#define M_PIf (3.14f)

#define FUZZYWIDTH (PAPERWIDTH*100)
float qsnap=0.25f;
#define QUANTIZEDCHECK (qsnap*(1.0f/256.0f * 0.5f))

int currentmaterial = 0;
float texscale=1.0f;


void texplanar(Face & face)
{
	float3 n(face.xyz());
	if(fabsf(n.x)>fabsf(n.y) && fabsf(n.x)>fabsf(n.z)) {
		face.gu = float3(0,(n.x>0.0f)?1.0f:-1.0f,0);
		face.gv = float3(0,0,1);
	}
	else if(fabsf(n.y)>fabsf(n.z)) {
		face.gu = float3((n.y>0.0f)?-1.0f:1.0f,0,0);
		face.gv = float3(0,0,1);
	}
	else {
		face.gu = float3(1,0,0);
		face.gv = float3(0,(n.z>0.0f)?1.0f:-1.0f,0);
	}
	face.gu *= texscale;
	face.gv *= texscale;
}
void AssignTex(Face& face)
{
	texplanar(face);
}

void AssignTex(BSPNode *node,int matid)
{
	if(!node) {
		return;
	}
	for(auto & face : node->brep)
    {
		face.matid = matid;
		AssignTex(face);
	}
	AssignTex(node->under.get(), matid);
	AssignTex(node->over.get(), matid);
}





void FaceExtractMatVals(Face *face,
	                    const float3  &v0,const float3  &v1,const float3  &v2,
	                    const float2 &t0,const float2 &t1,const float2 &t2) 
{
	face->gu = gradient(v0,v1,v2,t0.x,t1.x,t2.x);
	face->gv = gradient(v0,v1,v2,t0.y,t1.y,t2.y);
	face->ot.x =  t0.x - dot(v0,face->gu);
	face->ot.y =  t0.y - dot(v0,face->gv);
}

void texcylinder(Face *face)
{
	face->gv = float3(0,0,1);
	face->gu = normalize(cross(face->gv,face->xyz()));
	float3 sa=face->vertex[maxdir(face->vertex,-face->gu)];
	float3 sb=face->vertex[maxdir(face->vertex, face->gu)];
	sa.z=0;
	sb.z=0;
	float ta = atan2f(sa.x,sa.y)/(2.0f*M_PIf);
	float tb = atan2f(sb.x,sb.y)/(2.0f*M_PIf);
	if(ta < tb-0.5f) ta+= 1.0f;
	if(tb < ta-0.5f) tb+= 1.0f;
	FaceExtractMatVals(face,sa,sb,sb+float3(0,0,1.0f),float2(ta,0),float2(tb,0),float2(tb,1));
//	float3 so=LineProject(sa,sb,face->normal * -face->dist);
	//float to = atan2f(face->normal.x,face->normal.y)/(2.0f*PI);
//	float to = ta + (tb-ta) * magnitude(so-sa)/magnitude(sb-sa);
//	face->gu *= (tb-ta)/magnitude(sa-sb);
//	face->gv *= magnitude(face->gu);
//	face->ot = float3(to,0,0);
}

float2 FaceTexCoord(Face *f,int i)
{
	return float2(f->ot.x+dot(float3(f->vertex[i]),f->gu),f->ot.y+dot(float3(f->vertex[i]),f->gv));
}

float2 FaceTexCoord(Face *f,const float3 &v)
{
	return float2(f->ot.x+dot(v,f->gu),f->ot.y+dot(v,f->gv));
}

Face FaceNewQuad(const float3 &v0,const float3 &v1,const float3 &v2,const float3 &v3)
{
	Face f;
    f.vertex = {v0,v1,v2,v3};
	f.xyz() = normalize(cross(v1 - v0, v2 - v1) + cross(v3 - v2, v0 - v3));
	f.w = -dot(f.xyz(), (v0 + v1 + v2 + v3) / 4.0f);
    for(auto & v : f.vertex) assert(!PlaneTest(f.plane(), v, PAPERWIDTH)); // must be coplanar
	FaceExtractMatVals(&f,v0,v1,v3,float2(0,0),float2(1,0),float2(0,1));
	f.gu = normalize(f.gu);
	f.gv = normalize(f.gv);
	f.ot = float3(0,0,0);
	return f;
}

Face FaceNewTri(const float3 &v0,const float3 &v1,const float3 &v2)
{
	Face f;
    f.vertex = {v0,v1,v2};
	f.xyz() = safenormalize(cross(v1 - v0, v2 - v1));
	f.w = -dot(f.xyz(), (v0 + v1 + v2) / 3.0f);
	f.gu = safenormalize(v1-v0);
	f.gv = safenormalize(cross(f.xyz(), f.gu));
	return f;
}

Face FaceNewTriTex(const float3 &v0,const float3 &v1,const float3 &v2,const float2 &t0,const float2 &t1,const float2 &t2)
{
	Face f;
    f.vertex = {v0,v1,v2};
	f.xyz() = TriNormal(v0,v1,v2) ;
	f.w = -dot(f.xyz(), (v0 + v1 + v2) / 3.0f);
	FaceExtractMatVals(&f,v0,v1,v2,t0,t1,t2);
	return f;
}

float FaceArea(const Face & face){
	float area=0;
	for(unsigned int i=2;i<face.vertex.size();i++) {
		const float3 &vb=face.vertex[0];
		const float3 &v1=face.vertex[i-1];
		const float3 &v2=face.vertex[i];
		area += dot(face.xyz(), cross(v1 - vb, v2 - v1)) / 2.0f;
	}
	return area;

}

float3 FaceCenter(const Face & face)
{
	float3 centroid(0,0,0); // ok not really, but ill make a better routine later!
	for(unsigned int i=0;i<face.vertex.size();i++) {
		centroid = centroid + face.vertex[i];
	}
	return centroid/(float)face.vertex.size();
}

void FaceTranslate(Face & face, const float3 &offset){
	for(auto & v :  face.vertex) v += float3(offset);
    PlaneTranslate(face.plane(), offset);
	face.ot.x -= dot(offset,face.gu);
	face.ot.y -= dot(offset,face.gv);
}
 
void FaceRotate(Face & face, const float4 &r)
{
	for(auto & v : face.vertex) v = qrot(r,v);
    PlaneRotate(face.plane(), r);
	face.gu = qrot(r,face.gu);
	face.gv = qrot(r,face.gv);
	face.ot = qrot(r,face.ot);
}

void FaceScale(Face & face, const float3 & scaling)
{
    for(auto & v : face.vertex) v = cmul(v, scaling);
    PlaneScale(face.plane(), scaling);
}

void FaceScale(Face & face, float scaling)
{
    for(auto & v : face.vertex) v *= scaling;
    PlaneScale(face.plane(), scaling);
    // TODO: Does scaling affect face.ot? 
}

void FaceTranslate(std::vector<Face> & faces,const float3 & offset) {
	for(auto & f : faces) {
		FaceTranslate(f,offset);
	}
}

int FaceClosestEdge(Face *face,const float3 &sample_point)
{
	// sample_point assumed to be an interior point
	assert(face->vertex.size()>=3);
	int closest=-1;
	float mindr;
	for(unsigned int i=0;i<face->vertex.size();i++)
	{
		unsigned int i1 = (i+1)%face->vertex.size();
		float3 v0(face->vertex[i ]);
		float3 v1(face->vertex[i1]);
		float d = magnitude(LineProject(v0,v1,sample_point)-sample_point);
		if(closest==-1 || d<mindr)
		{
			mindr=d;
			closest = i;
		}
	}
	assert(closest>=0);
	return closest;
}


void FaceSlice(Face & face,const float4 &clip) {
	int count=0;
	for(unsigned int i=0;i<face.vertex.size();i++) {
		unsigned int i2=(i+1)%face.vertex.size();
		int   vf0 = PlaneTest(clip, face.vertex[i]);
		int   vf2 = PlaneTest(clip, face.vertex[i2]);
		if((vf0==OVER  && vf2==UNDER)||
		   (vf0==UNDER && vf2==OVER )  )
		{
			float3 vmid;
			vmid = PlaneLineIntersection(clip,face.vertex[i],face.vertex[i2]);
			assert(!PlaneTest(clip, vmid));
			face.vertex.insert(face.vertex.begin() + i2, vmid); //  Insert(face.vertex, vmid, i2);
			i=0;	
			assert(count<2);
			count++;
		}
	}
}

Face FaceClip(const Face & face, const float4 & clip) { return FaceClip(Face(face), clip); }
Face FaceClip(Face && face,const float4 &clip) {
	assert(FaceSplitTest(face, clip) == SPLIT);
	FaceSlice(face,clip);
	std::vector<float3> tmp;
	for(unsigned int i=0;i<face.vertex.size();i++){
		if (PlaneTest(clip, face.vertex[i]) != OVER) {
			tmp.push_back(face.vertex[i]);
		}
	}
	face.vertex.clear();
	for (unsigned int i = 0; i<tmp.size(); i++){
		face.vertex.push_back(tmp[i]);
	}
	return face;
}

int FaceSplitTest(const Face & face, const float4 &splitplane,float epsilon)
{
	int flag = COPLANAR;  // 0
	for (const auto & v : face.vertex) flag |= PlaneTest(splitplane, v, epsilon);
	return flag;
}

void  FaceSliceEdge(Face *face,int e0,BSPNode *n) {
	if(n->isleaf) {
		return;
	}
	int e1 = (e0+1) % face->vertex.size();
	assert(0);
	// fill this in!!!
}

int edgesplitcount=0;
static void FaceEdgeSplicer(Face & face,int vi0,BSPNode *n)
{
	// the face's edge starting from vertex vi0 is sliced by any incident hypeplane in the bsp
	if(n->isleaf) return;
	int vi1 = (vi0+1)%face.vertex.size();
	float3 v0 = face.vertex[vi0];
	float3 v1 = face.vertex[vi1];
	assert(magnitude(v0-v1) > QUANTIZEDCHECK );
	int f0 = PlaneTest(float4(n->xyz(), n->w), v0);
	int f1 = PlaneTest(float4(n->xyz(), n->w), v1);
	if(f0==COPLANAR && f1== COPLANAR)
	{
		// have to pass down both sides, but we have to make sure we do all subsegments generated by the first side
		int count = face.vertex.size();
		FaceEdgeSplicer(face,vi0,n->under.get());
		int k=vi0 + (face.vertex.size()-count);
		while(k>=vi0)
		{
			FaceEdgeSplicer(face,k,n->over.get());
			k--;
		}
	}
	else if((f0|f1) == UNDER)
	{
		FaceEdgeSplicer(face,vi0,n->under.get());
	}
	else if((f0|f1) == OVER)
	{
		FaceEdgeSplicer(face,vi0,n->over.get());
	}
	else
	{
		edgesplitcount++;
		assert(magnitude(v0-v1) > QUANTIZEDCHECK);
		assert((f0|f1) == SPLIT);
		float3 vmid = PlaneLineIntersection(*n,v0,v1);
		assert(magnitude(vmid-v1) > QUANTIZEDCHECK);
		assert(magnitude(v0-vmid) > QUANTIZEDCHECK);
		int fmid = PlaneTest(float4(n->xyz(), n->w), vmid);
		assert(fmid==0);
		face.vertex.insert(face.vertex.begin() + vi0 + 1, vmid); //  Insert(face.vertex, vmid, vi0 + 1);
		if(f0==UNDER)
		{
			FaceEdgeSplicer(face,vi0+1,n->over.get());
			FaceEdgeSplicer(face,vi0  ,n->under.get());
		}
		else
		{
			assert(f0==OVER);
			FaceEdgeSplicer(face,vi0+1,n->under.get());
			FaceEdgeSplicer(face,vi0  ,n->over.get());
		}
	}
	
}


int FaceSplitifyEdges(BSPNode *root)  // tests (possibly splits) all brep edges O(n lg(n))-ish 
{
	edgesplitcount=0;
	for (auto n : treetraverse(root))
	{
		if(!n) 
			continue; // shouldn't happen
		for (auto &face: n->brep)
		{
			int j=face.vertex.size();      // in reverse order since we may end up inserting into this array
			while(j--)                         // for every edge of every brep face...
				FaceEdgeSplicer(face,j,root);  // starting at root, pass the edge down the (relevant nodes of) tree 
		}
	}
	return edgesplitcount;
}

void FaceSanityCheck(Face *face)
{
	for(unsigned int i=0;i<face->vertex.size();i++)
	{
		int i1 = (i+1)%face->vertex.size();
		assert(magnitude(face->vertex[i1]-face->vertex[i])>QUANTIZEDCHECK);
	}
}

//std::vector<Face> deadfaces;
void FaceEmbed(BSPNode *node, Face && face) {
	assert(node);
	if(node->isleaf==OVER) {
		//deadfaces.push_back(std::move(face));
		return;
	}
	if(node->isleaf==UNDER) {
		node->brep.push_back(std::move(face));
		return;
	}
	int flag = FaceSplitTest(face, node->plane());
	if(flag==UNDER) {
		FaceEmbed(node->under.get(),std::move(face));
		return;
	}
	if(flag==OVER) {
		FaceEmbed(node->over.get(),std::move(face));
		return;
	}
	if(flag==COPLANAR) {
		FaceEmbed(dot(node->xyz(), face.xyz()) > 0 ? node->under.get() : node->over.get(), std::move(face));
		return;
	}
	assert(flag==SPLIT);

	// FIXME:  add FaceSliceEdge calls here!
	FaceEmbed(node->over.get(), FaceClip(face, -node->plane()));
	FaceEmbed(node->under.get(), FaceClip(std::move(face), node->plane()));
}



/*static void GenerateFaces(BSPNode *n) {
	assert(root);
	assert(n);
	if(n->isleaf==UNDER) {
		return;
	}
	if(n->isleaf==OVER) {
		int i;
		assert(n->cell);
		for(i=0;i<n->cell->face.size();i++) {
			Face *face = FaceMakeBack(n->cell->face[i]);
			FaceEmbed(root,face);
		}
		return;
	}
	GenerateFaces(n->under);
	GenerateFaces(n->over);

}
*/


std::vector<Face> GenerateFacesReverse(const WingMesh &m)
{
	std::vector<Face> flist;
	for(unsigned int i=0;i<m.faces.size();i++)
	{
		Face f;
		f.plane() = -m.faces[i];
		extern int currentmaterial;
		f.matid=currentmaterial;
		int e0 = m.fback[i];
		int e=e0;
		do {
			f.vertex.push_back(m.verts[m.edges[e].v]);
			e = m.edges[e].prev;
		} while (e!=e0);
		AssignTex(f);
		flist.push_back(std::move(f));
	}
	return flist;
}

std::vector<Face> GenerateFaces(const WingMesh &m)
{
	std::vector<Face> flist;
	for(unsigned int i=0;i<m.faces.size();i++)
	{
		Face f;
		f.plane() = m.faces[i];
		extern int currentmaterial;
		f.matid=currentmaterial;
		int e0 = m.fback[i];
		int e=e0;
		do {
			f.vertex.push_back(m.verts[m.edges[e].v]);
			e = m.edges[e].next;
		} while (e!=e0);
		AssignTex(f);
		flist.push_back(std::move(f));
	}
	return flist;
}

static void GenerateFaces(BSPNode *root) 
{
	assert(root);
	for (auto n : treetraverse(root))
		if(n->isleaf==OVER) 
			for (auto &f : GenerateFacesReverse(n->convex))
				FaceEmbed(root, std::move(f));
}





static void ExtractMat(Face & face,const Face & src) 
{
	if (dot(face.xyz(), src.xyz())<0.95f) {
		return;
	}
	if (FaceSplitTest(face, src.plane(), PAPERWIDTH) != COPLANAR) {
		return;
	}
	float3 interior(0,0,0);
	for(auto &v : face.vertex) 
		interior += v * (1.0f/face.vertex.size());

	if (!PolyHitCheck(src.vertex, interior + face.xyz(), interior - face.xyz())){
		return;
	}
	// src and face coincident
	face.matid = src.matid;
	face.gu    = src.gu;
	face.gv    = src.gv;
	face.ot    = src.ot;
}

static void ExtractMat(BSPNode *n,const Face &poly) {
	for(unsigned int i=0;i<n->brep.size();i++) {
		ExtractMat(n->brep[i],poly);		
	}
	if(n->isleaf) {
		return;
	}
	int flag = FaceSplitTest(poly, n->plane());
	if(flag==COPLANAR) {
		ExtractMat((dot(n->xyz(), poly.xyz())>0) ? n->under.get() : n->over.get(), poly);
		return;
	}
	if(flag & UNDER) {
		ExtractMat(n->under.get(),poly);
	}
	if(flag & OVER) {
		ExtractMat(n->over.get() ,poly);
	}
}


std::vector<Face> BSPRipBrep(BSPNode *root)
{
	std::vector<Face> faces;
	for (auto n : treetraverse(root))
		while(n->brep.size())
			faces.push_back(Pop(n->brep));
	return faces;
}

Face* NeighboringEdgeFace;
int   NeighboringEdgeId;
BSPNode *NeighboringEdgeNode;
Face *NeighboringEdge(BSPNode *root,Face *face,int eid)
{
	float3 &v0 = face->vertex[eid];
	float3 &v1 = face->vertex[(eid+1)%face->vertex.size()];
	assert(v0 != v1);
	float3 s = (v0+v1)/2.0f;
	float3 fnxe = cross((v1 - v0), face->xyz());  // points away from face's interior
	float4 p0 = *face;
	int f;
	while ((f = PlaneTest(float4(root->xyz(), root->w), s)))
	{
		root=(f==OVER)?root->over.get():root->under.get();
		assert(!root->isleaf);
	}
	std::vector<BSPNode *> stack;
	stack.push_back(root);
	while(stack.size())
	{
		BSPNode *n=Pop(stack);
		if(n->isleaf==OVER) continue;
		if(n->isleaf==UNDER)
		{
			for(unsigned int i=0;i<n->brep.size();i++)
			{
				Face *f = &n->brep[i];
				if(f==face) continue;
				for(unsigned int j=0;j<f->vertex.size();j++)
				{
					int j1 = (j+1)%f->vertex.size();
					if(magnitude(f->vertex[j]-v1)<0.001f && magnitude(f->vertex[j1]-v0)<0.001f)
					{
						NeighboringEdgeNode=n;
						NeighboringEdgeId=j;
						NeighboringEdgeFace=f;
						return f;
					}
				}
			}
			continue;
		}
		assert(!n->isleaf);
		f = PlaneTest(float4(n->xyz(), n->w), s);
		if(!(f&OVER))  stack.push_back(n->under.get());
		if(!(f&UNDER)) stack.push_back(n->over.get());
	}
	assert(0);
	return NULL;
}

void BSPMakeBrep(BSPNode *r, std::vector<Face> && faces)
{
	GenerateFaces(r);
	FaceSplitifyEdges(r);
	for(auto & f : faces) 
		ExtractMat(r,f);
}

static void BSPClipFace(BSPNode *n,Face && face,const float3 &position,std::vector<Face> &under,std::vector<Face> &over)
{
	if(n->isleaf==UNDER)
	{
		under.push_back(face);
		return;
	}
	if(n->isleaf==OVER)
	{
		over.push_back(face);
		return;
	}
	float4 plane(n->xyz(), n->w + dot(position, n->xyz()));
	int flag = FaceSplitTest(face, plane);
	if(flag == UNDER) {
		return BSPClipFace(n->under.get(),std::move(face),position,under,over);
	}
	if(flag == OVER) {
		return BSPClipFace(n->over.get(),std::move(face),position,under,over);
	}
	if(flag==COPLANAR)
	{
		return BSPClipFace(dot(n->xyz(), face.xyz()) > 0 ? n->under.get() : n->over.get(), std::move(face), position, under, over);
	}
	assert(flag==SPLIT);
	
	Face funder, fover;
	fover.xyz() = funder.xyz() = face.xyz();
	fover.w  = funder.w = face.w;
	fover.gu   = funder.gu   = face.gu;
	fover.gv   = funder.gv   = face.gv;
	fover.ot   = funder.ot   = face.ot;
	fover.matid= funder.matid= face.matid;
	for(unsigned int i=0;i<face.vertex.size();i++){
		float3& vi = face.vertex[i];
		float3& vi1= face.vertex[(i+1)%face.vertex.size()];
		int vf  = PlaneTest(float4(plane.xyz(), plane.w), vi);
		int vf1 = PlaneTest(float4(plane.xyz(), plane.w), vi1);
		if(vf==COPLANAR) 
		{
			funder.vertex.push_back(vi);
			fover.vertex.push_back(vi);
			continue;   // possible loop optimization
		}
		else if(vf==UNDER)
		{
			funder.vertex.push_back(vi);
		}
		else 
		{
			assert(vf==OVER);
			fover.vertex.push_back(vi);
		}
		if(vf != vf1 && vf !=COPLANAR && vf1 != COPLANAR)
		{
			float3 vmid = PlaneLineIntersection(plane.xyz(), plane.w, vi, vi1);
			funder.vertex.push_back(vmid);
			fover.vertex.push_back(vmid);
		}
	}
	BSPClipFace(n->under.get(),std::move(funder),position,under,over);
	BSPClipFace(n->over.get() ,std::move(fover) ,position,under,over);
}

void BSPClipFaces(BSPNode *bsp, std::vector<Face> && faces,const float3 &position,std::vector<Face> &under,std::vector<Face> &over)
{
	for(unsigned int i=0;i<faces.size();i++)
		BSPClipFace(bsp, std::move(faces[i]), position, under, over);
}