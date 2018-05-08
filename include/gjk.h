//
// gjk algorithm
// by stan melax 2007   bsd licence
//
// Just a basic gjk implementation using some wisdom described in Gino's papers on the algorithm. 
// Essecntially take two convex shapes and their poses and return separating plane.
//
// For the moving version, Jay Stelly gets credit for the brilliant  tunnel idea to 
// search the polytope in reverse to determine time of impact.  
// Idea perhaps not published, but might be described on molly rocket or bullet forums.
//

#include <assert.h>
#include <functional>

#include "vecmatquat_minimal.h"

// still reorganizing little geometry functions, putting these here for now:

inline float3 NormalOf(const float3 &v0, const float3 &v1, const float3 &v2)
{
	return normalize(cross(v2 - v0, v2 - v1));
}

inline float3 PlaneLineIntersection(const float3 &normal, const float dist, const float3 &p0, const float3 &p1)
{
	// returns the point where the line p0-p1 intersects the plane n&d
	float3 dif;
	dif = p1 - p0;
	float dn = dot(normal, dif);
	float t = -(dist + dot(normal, p0)) / dn;
	return p0 + (dif*t);
}

inline float3 LineProject(const float3 &p0, const float3 &p1, const float3 &a)
{
	// project point a on segment [p0,p1]
	float3 d = p1 - p0;
	float t = dot(d, (a - p0)) / dot(d, d);
	return p0 + d*t;
}


inline float LineProjectTime(const float3 &p0, const float3 &p1, const float3 &a)
{
	// project point a on segment [p0,p1]
	float3 d = p1 - p0;
	float t = dot(d, (a - p0)) / dot(d, d);
	return t;
}
inline float3 BaryCentric(const float3 &v0, const float3 &v1, const float3 &v2, float3 s)
{
	float3x3 m(v0, v1, v2);
	if (determinant(m) == 0)
	{
		int k = (magnitude(v1 - v2)>magnitude(v0 - v2)) ? 1 : 0;
		float t = LineProjectTime(v2, m[k], s);
		return float3((1 - k)*t, k*t, 1 - t);
	}
	return mul(inverse(m), s);
}
inline int tri_interior(const float3& v0, const float3& v1, const float3& v2, const float3& d)
{
	float3 b = BaryCentric(v0, v1, v2, d);
	return (b.x >= 0.0f && b.y >= 0.0f && b.z >= 0.0f);
}


inline  float3 PlaneProjectOf(const float3 &v0, const float3 &v1, const float3 &v2, const float3 &point)
{
	float3 cp = cross(v2 - v0, v2 - v1);
	float dtcpm = -dot(cp, v0);
	float cpm2 = dot(cp, cp);
	if (cpm2 == 0.0f)
	{
		return LineProject(v0, (magnitude(v1 - v0)>magnitude(v2 - v0)) ? v1 : v2, point);
	}
	return point - cp * (dot(cp, point) + dtcpm) / cpm2;
}



class Contact
{
public:
	//Collidable*		C[2];
	int				type; //  v[t] on c[0] for t<type on c[1] for t>=type so 2 is edge-edge collision
	float3			v[4];
	float3			normal; // worldspace  points from C[1] to C[0]
	float			dist;  // plane
	float3			impact;
	float			separation;
	float3 p0, p1;
	float3 p0w, p1w;
	float time;
	Contact() :normal(0, 0, 0), impact(0, 0, 0) { type = -1; separation = FLT_MAX; }
	operator bool() { return separation <= 0; }
};

struct MKPoint
{
	float3 a;
	float3 b;
	float3 p;
	float t;
	MKPoint(const float3  &a, const float3  &b, const float3& p) :a(a), b(b), p(p), t(0){}
	MKPoint(){}
};




struct MinkSimplex
{
	float3 v;
	MKPoint W[4];
	int count;
	float3 pa;
	float3 pb;
};

inline int operator==(const MKPoint &a,const MKPoint &b)
{
	return (a.a==b.a && a.b==b.b);
}

inline MKPoint PointOnMinkowski(std::function<float3(const float3&)> A, std::function<float3(const float3&)>B,  const float3 &n)
{
	auto a = A( n);
	auto b = B(-n);
	return MKPoint(a,b,a-b);
}
inline MKPoint PointOnMinkowski(std::function<float3(const float3&)> A, std::function<float3(const float3&)>B, const float3& ray, const float3 &n)
{
	auto p = PointOnMinkowski(A, B, n);
	p.p += ((dot(n, ray) > 0.0f) ? ray : float3(0, 0, 0));
	return p;
}





inline int NextMinkSimplex0(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w)
{
	assert(src.count==0);
	dst.W[0] = w;
	dst.W[0].t = 1.0f;
	dst.v = w.p;
	// ?? dst.pa = w.a;
	dst.count=1;
	return 1;
}

inline int NextMinkSimplex1(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w)
{
	assert(src.count==1);
	float t = LineProjectTime(w.p ,src.W[0].p,  float3(0,0,0));
	if(t<0.0f)  // w[0] is useless
	{
		dst.W[0] = w;
		dst.W[0].t=1.0f;
		dst.v = w.p;
		dst.count=1;
		return 1;
	}
	dst.W[0] = src.W[0];
	dst.W[0].t = t;
	dst.W[1] = w;
	dst.W[1].t = 1.0f-t;
	dst.v = w.p + (src.W[0].p-w.p)*t;
	dst.count=2;
	return 1;
}

inline int NextMinkSimplex2(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w)
{
	assert(src.count==2);
	const float3 &w0 = src.W[0].p;
	const float3 &w1 = src.W[1].p;
	float t0 = LineProjectTime(w.p ,w0,  float3(0,0,0));
	float t1 = LineProjectTime(w.p ,w1,  float3(0,0,0));
	float3 v0 = w.p + (w0 - w.p) * t0;
	float3 v1 = w.p + (w1 - w.p) * t1;
	int ine0 = (dot(-v0,w1-v0)>0.0f);
	int ine1 = (dot(-v1,w0-v1)>0.0f);

	if(ine0 && ine1)
	{
		dst.count=3;
		float3 v = PlaneProjectOf(w0,w1,w.p,float3(0,0,0));
		dst.v=v;
		dst.W[0] = src.W[0];
		dst.W[1] = src.W[1];
		dst.W[2] = w;
		//float3 b = BaryCentric(w0,w1,w.p, v); // probably dont need to compute this unless we need pa and pb
		//dst.W[0].t = b.x;
		//dst.W[1].t = b.y;
		//dst.W[2].t = b.z;
		return 1;
	}
	if(!ine0 && (t0>0.0f)) 
	{
		// keep w0
		dst.W[0] = src.W[0];
		dst.W[0].t = t0;
		dst.W[1] = w;
		dst.W[1].t = 1.0f-t0;
		dst.v = v0 ;
		dst.count=2;
		return 1;
	}
	if(!ine1 && (t1>0.0f))
	{
		// keep w1
		dst.W[0] = src.W[1];
		dst.W[0].t = t1;
		dst.W[1] = w;
		dst.W[1].t = 1.0f-t1;
		dst.v = v1;
		dst.count=2;
		return 1;
	}
	// w by itself is the closest feature
	dst.W[0] = w;
	dst.W[0].t=1.0f;
	dst.v = w.p;
	dst.count=1;
	return 1;
}

inline int NextMinkSimplex3(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w)
{
	assert(src.count==3);
	const float3 &w0 = src.W[0].p;
	const float3 &w1 = src.W[1].p;
	const float3 &w2 = src.W[2].p;
	float t[3];
	float3 v[3];
	float3 vc[3];
	t[0] = LineProjectTime(w.p ,w0,  float3(0,0,0));
	t[1] = LineProjectTime(w.p ,w1,  float3(0,0,0));
	t[2] = LineProjectTime(w.p ,w2,  float3(0,0,0));
	v[0]  = w.p + (w0 - w.p) * t[0];
	v[1]  = w.p + (w1 - w.p) * t[1];
	v[2]  = w.p + (w2 - w.p) * t[2];
	vc[0] = PlaneProjectOf(w.p,w1,w2, float3(0,0,0));  // project point onto opposing face of tetrahedra
	vc[1] = PlaneProjectOf(w.p,w2,w0, float3(0,0,0));
	vc[2] = PlaneProjectOf(w.p,w0,w1, float3(0,0,0));
	int inp0 = (dot(-vc[0],w0-vc[0])>0.0f) ;
	int inp1 = (dot(-vc[1],w1-vc[1])>0.0f) ;
	int inp2 = (dot(-vc[2],w2-vc[2])>0.0f) ;
	
	// check volume
	if(inp0 && inp1 && inp2)
	{
		// got a  simplex/tetrahedron that contains the origin
		dst=src;
		dst.count=4;
		dst.v= float3(0,0,0);
		dst.W[3]=w;
		return 0;
	}

	// check faces
	int inp2e0 = (dot(-v[0],w1-v[0])>0.0f);
	int inp2e1 = (dot(-v[1],w0-v[1])>0.0f);
	if(!inp2 && inp2e0 && inp2e1)
	{
		dst.count=3;
		float3 v = PlaneProjectOf(w0,w1,w.p,float3(0,0,0));
		dst.v=v;
		dst.W[0] = src.W[0];
		dst.W[1] = src.W[1];
		dst.W[2] = w;
		return 1;
	}
	int inp0e1 = (dot(-v[1],w2-v[1])>0.0f);
	int inp0e2 = (dot(-v[2],w1-v[2])>0.0f);
	if(!inp0 && inp0e1 && inp0e2)
	{
		dst.count=3;
		float3 v = PlaneProjectOf(w1,w2,w.p,float3(0,0,0));
		dst.v=v;
		dst.W[0] = src.W[1];
		dst.W[1] = src.W[2];
		dst.W[2] = w;
		return 1;
	}
	int inp1e2 = (dot(-v[2],w0-v[2])>0.0f);
	int inp1e0 = (dot(-v[0],w2-v[0])>0.0f);
	if(!inp1 && inp1e2 && inp1e0)
	{
		dst.count=3;
		float3 v = PlaneProjectOf(w2,w0,w.p,float3(0,0,0));
		dst.v=v;
		dst.W[0] = src.W[2];
		dst.W[1] = src.W[0];
		dst.W[2] = w;
		return 1;
	}

	// check ridges
	if(!inp1e0 && !inp2e0 && t[0]>0.0f)
	{
		dst.W[0] = src.W[0]; // keep w0
		dst.W[0].t = t[0];
		dst.W[1] = w;
		dst.W[1].t = 1.0f-t[0];
		dst.v = v[0] ;
		dst.count=2;
		return 1;
	}
	if(!inp2e1 && !inp0e1 && t[1]>0.0f)
	{
		dst.W[0] = src.W[1]; // keep w1
		dst.W[0].t = t[1];
		dst.W[1] = w;
		dst.W[1].t = 1.0f-t[1];
		dst.v = v[1] ;
		dst.count=2;
		return 1;
	}
	if(!inp0e2 && !inp1e2 && t[2]>0.0f)
	{
		dst.W[0] = src.W[2]; // keep w2
		dst.W[0].t = t[2];
		dst.W[1] = w;
		dst.W[1].t = 1.0f-t[2];
		dst.v = v[2] ;
		dst.count=2;
		return 1;
	}
	// w by itself is the closest feature
	dst.W[0] = w;
	dst.W[0].t=1.0f;
	dst.v = w.p;
	dst.count=1;
	return 1;

}

inline  void fillhitv(Contact &hitinfo,MinkSimplex &src)
{
	hitinfo.type=-1;
	if(src.count!=3) 
	{
		return;
	}
	if(src.W[0].a == src.W[1].a && src.W[0].a == src.W[2].a )
	{
		// point plane
		hitinfo.type = 1;
		hitinfo.v[0] = src.W[0].a;
		hitinfo.v[1] = src.W[0].b;
		hitinfo.v[2] = src.W[1].b;
		hitinfo.v[3] = src.W[2].b;
		if(dot(hitinfo.normal, cross(src.W[1].p-src.W[0].p,src.W[2].p-src.W[0].p))<0)
		{
			std::swap(hitinfo.v[1],hitinfo.v[2]);
		}
		return;
	}
	if(src.W[0].b == src.W[1].b && src.W[0].b == src.W[2].b )
	{
		// plane point
		hitinfo.type = 3;
		hitinfo.v[0] = src.W[0].a;
		hitinfo.v[1] = src.W[1].a;
		hitinfo.v[2] = src.W[2].a;
		hitinfo.v[3] = src.W[2].b;
		if(dot(hitinfo.normal, cross(src.W[1].p-src.W[0].p,src.W[2].p-src.W[0].p))<0)
		{
			std::swap(hitinfo.v[1], hitinfo.v[2]);
		}
		return;
	}
	if((src.W[0].a == src.W[1].a || src.W[0].a == src.W[2].a || src.W[1].a == src.W[2].a ) && 
	   (src.W[0].b == src.W[1].b || src.W[0].b == src.W[2].b || src.W[1].b == src.W[2].b ))
	{
		// edge edge
		hitinfo.type = 2;
		hitinfo.v[0] = src.W[0].a;
		hitinfo.v[1] = (src.W[1].a!=src.W[0].a)? src.W[1].a : src.W[2].a;
		hitinfo.v[2] = src.W[0].b;
		hitinfo.v[3] = (src.W[1].b!=src.W[0].b)? src.W[1].b : src.W[2].b;
		float dp = dot(hitinfo.normal, cross(src.W[1].p-src.W[0].p,src.W[2].p-src.W[0].p));
		if((dp<0 && src.W[1].a!=src.W[0].a)||(dp>0 && src.W[1].a==src.W[0].a))
		{
			std::swap(hitinfo.v[2], hitinfo.v[3]);
		}
		return;
	}
	if(src.W[0].b != src.W[1].b && src.W[0].b != src.W[2].b && src.W[1].b != src.W[2].b )
	{
	}
	if(src.W[0].a != src.W[1].a && src.W[0].a != src.W[2].a && src.W[1].a != src.W[2].a )
	{
	}
	// could be point point or point edge
	// therefore couldn't classify type to something that suggests a plane from available vertex indices.
}
inline  void calcpoints(std::function<float3(const float3&)> A, std::function<float3(const float3&)>B, Contact &hitinfo, MinkSimplex &src)
{
	int i;
	assert(src.count>0);
	if(src.count==3) // need to computed weights t
	{
		float3 b = BaryCentric(src.W[0].p,src.W[1].p,src.W[2].p,src.v);
		src.W[0].t=b.x;
		src.W[1].t=b.y;
		src.W[2].t=b.z;
	}
	src.pa=src.pb=float3(0,0,0);
	for(i=0;i<src.count;i++) 
	{
		src.pa += src.W[i].t * src.W[i].a;
		src.pb += src.W[i].t * src.W[i].b;
	}
	hitinfo.p0w = src.pa;
	hitinfo.p1w = src.pb;
	hitinfo.impact = (src.pa + src.pb)*0.5f;
	hitinfo.separation = magnitude(src.pa - src.pb);
	hitinfo.normal     =  normalize(src.v);
	hitinfo.dist       = -dot(hitinfo.normal,hitinfo.impact);
	fillhitv(hitinfo,src);
}

class MTri
{
public: 
	int w[3];
	float3 v;
	MTri(){w[0]=w[1]=w[2]=-1;}
	MTri(int w0,int w1,int w2,const float3 &_v){w[0]=w0;w[1]=w1;w[2]=w2;v=_v;}
	MTri(int w0,int w1,int w2,const std::vector<MKPoint> &W){w[0]=w0;w[1]=w1;w[2]=w2;
	                                                     v=PlaneProjectOf(W[w0].p,W[w1].p,W[w2].p,float3(0,0,0));
							float3 b = BaryCentric(W[w0].p,W[w1].p,W[w2].p,v);
								if(b.x<0 || b.y<0 || b.z<0 || b.x>1.0f || b.y>1.0f || b.z>1.0f) v=(W[w0].p + W[w1].p+W[w2].p)/3.0f;}
};

class Expandable
{
 public:
	std::vector<MKPoint> W;
	std::vector<MTri> tris;
	void expandit(MinkSimplex &dst, std::function<float3(const float3&)> A, std::function<float3(const float3&)>B);
	Expandable(const MinkSimplex &src);
};

inline Expandable::Expandable(const MinkSimplex &src)
{
	int i;
	assert(src.count==4);
	for(i=0;i<4;i++)
	{
		W.push_back(src.W[i]);
	}
	tris.push_back(MTri(0,1,2,W));
	tris.push_back(MTri(1,0,3,W));
	tris.push_back(MTri(2,1,3,W));
	tris.push_back(MTri(0,2,3,W));
}

inline void Expandable::expandit(MinkSimplex &dst, std::function<float3(const float3&)> A, std::function<float3(const float3&)>B )
{
	float prevdist = 0.0f;
	int done=0;
	while(1)
	{
		int closest=0;
		for(unsigned i=0;i<tris.size();i++)
		{
			if(magnitude(tris[i].v)<magnitude(tris[closest].v)) 
			{
				closest=i;
			}
		}
		float3 v = tris[closest].v;
		if(dot(v,v) <= prevdist)
		{
			done=1;
		}
		prevdist = dot(v,v);
		MKPoint w = PointOnMinkowski(A,B,v); 
		if(w== W[tris[closest].w[0]] ||w== W[tris[closest].w[1]] ||w== W[tris[closest].w[2]] )
		{
			done=1;
		}
		if(done ||  dot(w.p,v) < dot(v,v) + 0.0001) {
			//done
			dst.count=3;
			dst.v = v;
			dst.W[0] = W[tris[closest].w[0]];
			dst.W[1] = W[tris[closest].w[1]];
			dst.W[2] = W[tris[closest].w[2]];
			return;
		}
		int wi=W.size();
		W.push_back(w);
		tris.push_back(MTri(tris[closest].w[0],tris[closest].w[1],wi, W ));
		tris.push_back(MTri(tris[closest].w[1],tris[closest].w[2],wi, W ));
		tris.push_back(MTri(tris[closest].w[2],tris[closest].w[0],wi, W ));
		tris.erase(begin(tris) + closest);
	}
}


inline int Separated(std::function<float3(const float3&)> A, std::function<float3(const float3&)>B, Contact &hitinfo, int findclosest)
{
	int(*NextMinkSimplex[4])(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w) =
	{
		NextMinkSimplex0,
		NextMinkSimplex1,
		NextMinkSimplex2,
		NextMinkSimplex3
	};
	MinkSimplex last;
	MinkSimplex next;
	int iter=0;
	float3 v = PointOnMinkowski(A,B,float3(0,0,1)).p;
	last.count=0;
	last.v=v;
	MKPoint w = PointOnMinkowski(A,B,-v);
	NextMinkSimplex[0](next,last,w);
	last=next;
	// todo: add the use of the lower bound distance for termination conditions
	while((dot(w.p,v) < dot(v,v) - 0.00001f) && iter++<100) 
	{
		int isseparated;
		last=next;  // not ideal, a swapbuffer would be better
		v=last.v;
		w=PointOnMinkowski(A,B,-v);
		if(dot(w.p,v) >= dot(v,v)- 0.00001f - 0.00001f*dot(v,v))
		{
			//  not getting any closer here
			break;
		}
		if(!findclosest  &&  dot(w.p,v)>=0.0f) // found a separating plane
		{
			break;
		}
		isseparated = NextMinkSimplex[last.count](next,last,w);
		if(!isseparated)
		{
			Expandable expd(next);
			expd.expandit(last,A,B);
			float4 separationplane(0,0,0,0);
			separationplane.xyz() = NormalOf(last.W[0].p,last.W[1].p,last.W[2].p);
			if(dot(separationplane.xyz(),last.v)>0) separationplane.xyz() *=-1.0f;
			hitinfo.normal = separationplane.xyz();
			hitinfo.dist   = separationplane.w;
			float3 b = BaryCentric(last.W[0].p,last.W[1].p,last.W[2].p,last.v);
			last.W[0].t=b.x;
			last.W[1].t=b.y;
			last.W[2].t=b.z;
			last.pa = b.x * last.W[0].a + b.y * last.W[1].a + b.z*last.W[2].a;
			last.pb = b.x * last.W[0].b + b.y * last.W[1].b + b.z*last.W[2].b;
			hitinfo.p0w   =  last.pa;
			hitinfo.p1w   =  last.pb;
			hitinfo.impact=  (last.pa+last.pb )*0.5f;
			separationplane.w = -dot(separationplane.xyz(), hitinfo.impact);
			hitinfo.separation = -magnitude(last.pa - last.pb); //  dot(separationplane.normal,w.p-v);
			 
			fillhitv(hitinfo,last);
			return 0;
		}
		if(dot(next.v,next.v)>=dot(last.v, last.v))   // i.e. if magnitude(w.p)>magnitude(v) 
		{
			break;  // numerical screw up, 
		}
	}
	assert(iter<100);
	calcpoints(A,B,hitinfo,last);
	return 1;
}


inline  float tunnel(std::function<float3(const float3&)> A, std::function<float3(const float3&)>B, const float3& ray, MinkSimplex &start, Contact &hitinfo)
{
	int tet[4][3] = {{0,1,2},{1,0,3},{2,1,3},{0,2,3}};
	int i;
	MKPoint v0,v1,v2;
	int k=0;
	int s=-1;
	for(i=0;i<4;i++)
	{
		if(tri_interior(start.W[tet[i][0]].p,start.W[tet[i][1]].p,start.W[tet[i][2]].p,ray))
		{
			s=i;
			k++;
			v0 = start.W[tet[i][0]];
			v1 = start.W[tet[i][1]];
			v2 = start.W[tet[i][2]];
		}
	}
	assert(k==1);
	assert(s>=0);
	float3 n = NormalOf(v0.p,v1.p,v2.p);
	if(dot(n,ray)<0.0f)
	{
		n=-n;
		MKPoint tmp;
		tmp=v0;
		v0=v1;
		v1=tmp;
	}
	MKPoint v = PointOnMinkowski(A,B,ray,n);

	int iterlimit=0;
	while(iterlimit++<100 && v.p!=v0.p && v.p!= v1.p && v.p!=v2.p)
	{
		if(tri_interior(v0.p,v1.p,v.p,ray))
		{
			v2=v;
		}
		else if(tri_interior(v1.p,v2.p,v.p,ray))
		{
			v0=v;
		}
		else 
		{
			assert(tri_interior(v2.p,v0.p,v.p,ray));
			v1=v;
		}
		n = NormalOf(v0.p,v1.p,v2.p);
		v = PointOnMinkowski(A,B,ray,n);
		//v = mkp.p;
	}
	hitinfo.normal = -n;
	if(dot(hitinfo.normal,ray) > 0.0f)
	{
		hitinfo.normal *= -1.0f;  // i dont think this should happen, v0v1v2 winding should have been consistent
	}
	hitinfo.dist   = -dot(hitinfo.normal,v.p);
	float3 hitpoint = PlaneLineIntersection(hitinfo.normal,hitinfo.dist,float3(0,0,0),ray);
	float time = 1.0f - sqrtf(dot(hitpoint,hitpoint)/dot(ray,ray));
	hitinfo.time = time;
	float3 b = BaryCentric(v0.p,v1.p,v2.p, hitpoint);
	hitinfo.impact =  b.x * v0.b + b.y * v1.b + b.z*v2.b;
	return time;
}

inline int Sweep(std::function<float3(const float3&)>A, std::function<float3(const float3&)>B, const float3& dir, Contact &hitinfo)
{
	int(*NextMinkSimplex[4])(MinkSimplex &dst, const MinkSimplex &src, const MKPoint &w) =
	{
		NextMinkSimplex0,
		NextMinkSimplex1,
		NextMinkSimplex2,
		NextMinkSimplex3
	};
	MinkSimplex last;
	MinkSimplex next;
	int iter=0;
	float3 v = PointOnMinkowski(A,B,dir,float3(0,0,1)).p;
	last.count=0;
	last.v=v;
	MKPoint w = PointOnMinkowski(A,B,dir,-v);
	NextMinkSimplex[0](next,last,w);
	last=next;
	// todo: add the use of the lower bound distance for termination conditions
	while((dot(w.p,v) < dot(v,v) - 0.00001f) && iter++<100) 
	{
		int isseparated;
		last=next;  // not ideal, a swapbuffer would be better
		v=last.v;
		w=PointOnMinkowski(A,B,dir,-v);
		if(dot(w.p,v) >= dot(v,v)- 0.00001f - 0.00001f*dot(v,v))
		{
			//  not getting any closer here
			break;
		}
		if(dot(w.p,v)>=0.0f) // found a separating plane  // && !findclosest  
		{
			break;
		}
		isseparated = NextMinkSimplex[last.count](next,last,w);
		if(!isseparated)
		{
			// tunnel back and find the 
			float t=tunnel(A,B,dir,next,hitinfo);
			return 1;
		}
		if(dot(next.v,next.v)>=dot(last.v, last.v))   // i.e. if length(w.p)>length(v) 
		{
			break;  // numerical screw up, 
		}
	}
	assert(iter<100);
	calcpoints(A,B,hitinfo,last);
	return 0;
}
