// Pass 11A: geometry-only first REAL finite triangular mesh obstacle.
// Required: all VF/EE/piercing finite features, static and continuous
// source paths, positive and negative scenes. No fictitious physics
// response is claimed when the existing sphere solver hits this mesh.

#include "kyoto/triangle_mesh_contact.hpp"
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include "kyoto/time_consistent_contact.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace kyoto::solid;
static void expect(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
static bool nearly(double a,double b,double tol=1e-9){
    return std::abs(a-b)<=tol;
}
int main(int argc,char**argv){
 try{
    if(argc>2)throw std::invalid_argument(
      "usage: kyoto_triangle_mesh_tests [optional evidence folder]");
    const Triangle3 a={V3{-1,-1,0},V3{1,-1,0},
                       V3{0,1,0}};
    {
        const Triangle3 shifted={V3{-1,-1,.25},V3{1,-1,.25},
                                 V3{0,1,.25}};
        const auto d=triangle_triangle_closest(a,shifted);
        expect(nearly(d.distance,.25,1e-12),
               "parallel separated finite triangle distance wrong");
        const Triangle3 far={V3{3,3,2},V3{4,3,2},
                             V3{3,4,2}};
        expect(triangle_triangle_closest(a,far).distance>3,
               "3D finite outside-face distance lost");
    }
    {
        // Essential missed-by-naive-VF/EE case: one edge pierces
        // the INTERIOR of a triangle; no vertices are colliding
        // and all edges can be disjoint in their closest features.
        const Triangle3 needle={V3{0,0,-1},V3{0,0,1},
                                V3{.1,.25,1}};
        const auto d=triangle_triangle_closest(a,needle);
        expect(d.distance<1e-10,
               "triangle interior needle-piercing missed");
        expect(d.feature==RigidFaceContact::Feature::face_piercing,
               "triangle interior feature not classified as piercing");
    }
    {
        // Coplanar crossing edges, overlapping interiors and
        // disjoint finite triangles must be distinguished.
        const Triangle3 crossing={V3{-.8,.3,0},
            V3{.8,.3,0},V3{.2,-1.2,0}};
        expect(triangle_triangle_closest(a,crossing).distance<1e-11,
               "coplanar EE intersection was missed");
        const Triangle3 outside={V3{3,3,0},V3{4,3,0},
            V3{3,4,0}};
        expect(triangle_triangle_closest(a,outside).distance>2,
               "coplanar separated finite faces falsely intersect");
    }
    {
        const Triangle3 rigid={V3{-1,-1,0},V3{1,-1,0},
             V3{0,1,0}};
        const LinearTriangleMotion tunnel{
            {V3{-.1,0,1.5},V3{.1,0,1.5},V3{0,.2,1.5}},
            {V3{-.1,0,-1.5},V3{.1,0,-1.5},V3{0,.2,-1.5}}};
        auto s=certify_moving_triangle_rigid_triangle(
            tunnel,rigid,0.,11);
        expect(!s.entirely_separated&&s.safe_prefix<1 &&
               s.sampled_possible_contacts>0,
               "finite triangle continuous interior tunnelling missed");
        // Stationary and arbitrarily rotated finite surfaces.
        const LinearTriangleMotion safe{
            {V3{-.1,0,1.5},V3{.1,0,1.5},V3{0,.2,1.5}},
            {V3{-.1,0,1.5},V3{.1,0,1.5},V3{0,.2,1.5}}};
        auto q=certify_moving_triangle_rigid_triangle(safe,rigid,0.,0);
        expect(q.entirely_separated,
               "static actually separated faces left uncertain");
        const LinearTriangleMotion tangent{
            {V3{-.1,0,0},V3{.1,0,0},V3{0,.2,0}},
            {V3{-.1,0,0},V3{.1,0,0},V3{0,.2,0}}};
        expect(!certify_moving_triangle_rigid_triangle(
            tangent,rigid,0.,0).entirely_separated,
               "touching finite faces falsely certified strictly safe");
    }
    std::mt19937_64 rng(2026112311);
    std::uniform_real_distribution<double> xy(-.6,.6),
        z(1.1,3.3),delta(-.12,.12);
    std::size_t random_cert=0;
    for(int trial=0;trial<850;trial++){
        const Triangle3 target={V3{-1,-1,0},V3{1,-1,0},
                                V3{0,1,0}};
        LinearTriangleMotion motion;
        for(int i=0;i<3;i++){
            motion.start[i]={xy(rng),xy(rng),z(rng)};
            motion.end[i]=motion.start[i]+V3{
                delta(rng),delta(rng),delta(rng)};
        }
        const auto result=certify_moving_triangle_rigid_triangle(
           motion,target,.0005,9);
        expect(result.entirely_separated,
               "random genuinely far moving mesh collision uncertain");
        // Independent dense finite sample is a regression, not a CCD
        // theorem. A separate certificate proves the full interval.
        for(int k=0;k<=12;k++){
            const auto tri=lerp_vertices(
                motion.start,motion.end,k/12.);
            expect(triangle_triangle_closest(tri,target).distance>.0005,
                "certified 3D moving triangle has sampled collision");
        }
        random_cert++;
    }
    {
        // The NEGATIVE control must use the EXACT same finite rigid
        // obstacle as the new contact-response FEM experiment. The
        // old sphere solver has NO access to these 72 triangles.
        const StaticTriangleMesh mesh=corrugated_contact_patch();
        expect(mesh.vertices.size()==49&&mesh.faces.size()==72,
               "nonplanar obstacle topology invalid");
        // Auditing true physically integrated data from Pass 8
        // without magically claiming the sphere solver reacts to
        // this DIFFERENT rigid triangulated obstacle.
        Body body=toroidal_solid();
        const RigidSphere old_sphere{};
        auto initial=audit_deformable_rigid_mesh(body,mesh,0.);
        expect(initial.minimum_distance_m>0.01,
               "nonplanar obstacle intersects initial solid unexpectedly");
        std::size_t penetrating_states=0;
        std::size_t pair_contacts=0;
        double min_distance=std::numeric_limits<double>::infinity();
        std::ofstream csv;
        if(argc==2){
            std::filesystem::create_directories(argv[1]);
            csv.open(std::filesystem::path(argv[1])/
                "real_FEM_against_nonresponding_rigid_mesh.csv");
            csv<<std::setprecision(17);
            csv<<"step,time_s,exact_triangle_distance_m,"
                 "intersecting_face_pairs,paid_nearest_feature_tests,"
                 "AABB_lower_bound_prunes,physical_solver_has_mesh_response\n";
        }
        for(int step=0;step<=900;step++){
            if(step>0)step_time_consistent_whole_face_sphere(
                body,old_sphere,.00075);
            if(step%5!=0)continue;
            const auto a=audit_deformable_rigid_mesh(body,mesh,1e-8);
            min_distance=std::min(min_distance,a.minimum_distance_m);
            penetrating_states+=a.overlapping_face_pairs>0;
            pair_contacts+=a.overlapping_face_pairs;
            if(csv.is_open())
                csv<<step<<','<<body.time_seconds<<','
                   <<a.minimum_distance_m<<','
                   <<a.overlapping_face_pairs<<','
                   <<a.face_pair_tests<<','
                   <<a.broadphase_culled<<",false\n";
        }
        // IMPORTANT: a nonzero intersection would be a NEGATIVE
        // result proving the existing spherical response is NOT
        // adequate for finite nonplanar rigid triangle obstacles.
        std::cout<<"PASS11_MESH_NEGATIVE_CONTROL"
                 <<" old_sphere_solver_intersecting_frames="
                 <<penetrating_states
                 <<" total_intersecting_finite_face_pairs="
                 <<pair_contacts
                 <<" smallest_rigid_mesh_distance_m="
                 <<min_distance<<'\n';
    }
    expect(random_cert==850,"random separated mesh test incomplete");
    std::cout<<"PASS: finite 3D triangle/triangle VF, EE, interior "
                "piercing, continuous separation, 850 random motions "
                "and true nonresponding FEM/rigid-mesh audit\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}
