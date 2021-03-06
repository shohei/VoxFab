/*******************************************************************************
Copyright (c) 2016, Atsushi Masumori
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/


#include "VX_Voxelizer.h"

#if defined(_WIN32) || defined(_WIN64) //to get fmax, fmin to work on Windows/Visual Studio
#define fmax max
#define fmin min
#endif

CVXC_Voxelizer::CVXC_Voxelizer(){

}

CVXC_Voxelizer::~CVXC_Voxelizer(){
    delete voxel_data; voxel_data = nullptr;
}

int* CVXC_Voxelizer::Voxelize(float* vertices, int num_vertices, int* vox_num, float* unit_vox, bool is_fill_inside)
{

    // Calc length of bounding box of mesh model
//    float* p_aabb = getAABB(vertices, num_vertices); // axis aligned bounding box (min[0-3] and max[4-6] of vertex)
//    float aabb_length[3] = {
//        float(fabs(p_aabb[0] - p_aabb[3])),
//        float(fabs(p_aabb[1] - p_aabb[4])),
//        float(fabs(p_aabb[2] - p_aabb[5]))
//    };
//    delete p_aabb;

    // Set parameters.
    for(int i=0; i<3; ++i){
        voxel_size[i] = unit_vox[i];
        voxel_num[i] = vox_num[i];//ceil(aabb_length[i] / voxel_size[i])+2;
        voxel_origin[i] = +unit_vox[i]/2;//-aabb_length[i]/2;//-voxel_size[i]*voxel_num[i]*0.5f - voxel_size[i]*0.5f;
    }

    voxel_data = new int[voxel_num[0]*voxel_num[1]*voxel_num[2]];
    CVXC_VoxelizerUtil* c_detector = new CVXC_VoxelizerUtil;

    for(unsigned int i=0; i<num_vertices*3; i=i+9){

        float t_vertices[3][3];
        t_vertices[0][0] = vertices[i];
        t_vertices[0][1] = vertices[i+1];
        t_vertices[0][2] = vertices[i+2];
        t_vertices[1][0] = vertices[i+3];
        t_vertices[1][1] = vertices[i+4];
        t_vertices[1][2] = vertices[i+5];
        t_vertices[2][0] = vertices[i+6];
        t_vertices[2][1] = vertices[i+7];
        t_vertices[2][2] = vertices[i+8];

        float* tri_aabb = getTriAABB(t_vertices); // returned pointer is axis aligned bounding box (float[6]: min and max of vertex)
        float* target_index = getTargetIndexInAABB(tri_aabb, voxel_origin, voxel_size); // returned pointer is voxel aligned bounding box (float[6]: min and max of index)

        for(int z=target_index[2]; z<target_index[5]; z++){
            for(int y=target_index[1]; y<target_index[4]; y++){
                for(int x=target_index[0]; x<target_index[3]; x++){

                    float position[3] =
                    {
                        voxel_origin[0] + (x * voxel_size[0]),
                        voxel_origin[1] + (y * voxel_size[1]),
                        voxel_origin[2] + (z * voxel_size[2])
                    };

                    if(c_detector->is_collision(position, voxel_size, t_vertices)){
                        voxel_data[get_index(x,y,z)] = Collision_Cell;
                    }
                }
            }
        }
        delete tri_aabb;
        delete target_index;
    }

    if(is_fill_inside) fillInside();
    delete c_detector;
    return voxel_data;
}

int CVXC_Voxelizer::get_index(int x, int y, int z){

    return (voxel_num[0]*voxel_num[1]*z) + (voxel_num[0]*y) + x;

}

void CVXC_Voxelizer::fillInside(){

    int num_processed_cell = 1;

    // Set side voxels as Border_Cell;
    for(int z=0; z<voxel_num[2]; z++){
        for(int y=0; y<voxel_num[1]; y++){
            for(int x=0; x<voxel_num[0]; x++){

                int index = get_index(x,y,z);
                if( voxel_data[index] == Empty_Cell ){
                    if(x-1 < 0 || y-1 < 0 || z-1 < 0 || x+1 == voxel_num[0] || y+1 == voxel_num[1] || z+1 == voxel_num[2]){
                        voxel_data[index] = Processing_Cell;
                    }else{
                        voxel_data[index] = Pre_Process_Cell;
                    }
                }
            }
        }
    }

    // Expanding Border_Cell to Surface_Cell
    while(num_processed_cell > 0){

        num_processed_cell = 0;

        for(int z=0; z<voxel_num[2]; z++){
            for(int y=0; y<voxel_num[1]; y++){
                for(int x=0; x<voxel_num[0]; x++){

                    if( voxel_data[get_index(x,y,z)] == Processing_Cell ){
                        if(x+1 < voxel_num[0] && (voxel_data[get_index(x+1,y,z)] == Pre_Process_Cell)){ voxel_data[get_index(x+1,y,z)] = Processing_Cell; }
                        if(y+1 < voxel_num[1] && (voxel_data[get_index(x,y+1,z)] == Pre_Process_Cell)){ voxel_data[get_index(x,y+1,z)] = Processing_Cell; }
                        if(z+1 < voxel_num[2] && (voxel_data[get_index(x,y,z+1)] == Pre_Process_Cell)){ voxel_data[get_index(x,y,z+1)] = Processing_Cell; }
                        if(x-1 >= 0            && (voxel_data[get_index(x-1,y,z)] == Pre_Process_Cell)){ voxel_data[get_index(x-1,y,z)] = Processing_Cell; }
                        if(y-1 >= 0            && (voxel_data[get_index(x,y-1,z)] == Pre_Process_Cell)){ voxel_data[get_index(x,y-1,z)] = Processing_Cell; }
                        if(z-1 >= 0            && (voxel_data[get_index(x,y,z-1)] == Pre_Process_Cell)){ voxel_data[get_index(x,y,z-1)] = Processing_Cell; }

                        voxel_data[get_index(x,y,z)] = Processed_Cell;
                        num_processed_cell += 1;
                    }
                }
            }
        }
    }

    for(int i=0; i<voxel_num[0]*voxel_num[1]*voxel_num[2]; i++){
        if     ( voxel_data[i] == Pre_Process_Cell ){ voxel_data[i] = Inner_Cell; }
        else if( voxel_data[i] == Processed_Cell   ){ voxel_data[i] = Empty_Cell; }
    }
}

float* CVXC_Voxelizer::getAABB(float* vertices, int num_vertices){

    float* p_aabb = new float[6];

    float min_x = vertices[0];
    float min_y = vertices[0];
    float min_z = vertices[0];
    float max_x = vertices[0];
    float max_y = vertices[0];
    float max_z = vertices[0];

    for(int i=0; i<num_vertices; i++){
        if(min_x > vertices[i*3]) min_x = vertices[i*3];
        if(min_y > vertices[i*3+1]) min_y = vertices[i*3+1];
        if(min_z > vertices[i*3+2]) min_z = vertices[i*3+2];

        if(max_x < vertices[i*3]) max_x = vertices[i*3];
        if(max_y < vertices[i*3+1]) max_y = vertices[i*3+1];
        if(max_z < vertices[i*3+2]) max_z = vertices[i*3+2];
    }

    p_aabb[0] = min_x;
    p_aabb[1] = min_y;
    p_aabb[2] = min_z;

    p_aabb[3] = max_x;
    p_aabb[4] = max_y;
    p_aabb[5] = max_z;

    return p_aabb;
}

float* CVXC_Voxelizer::getTriAABB(float vertices[3][3])
{
    float* p_aabb = new float[6];
    p_aabb[0] = fmin(fmin(vertices[0][0],vertices[1][0]),vertices[2][0]); // min_x
    p_aabb[1] = fmin(fmin(vertices[0][1],vertices[1][1]),vertices[2][1]); // min_y
    p_aabb[2] = fmin(fmin(vertices[0][2],vertices[1][2]),vertices[2][2]); // min_z

    p_aabb[3] = fmax(fmax(vertices[0][0],vertices[1][0]),vertices[2][0]); // max_x
    p_aabb[4] = fmax(fmax(vertices[0][1],vertices[1][1]),vertices[2][1]); // max_y
    p_aabb[5] = fmax(fmax(vertices[0][2],vertices[1][2]),vertices[2][2]); // max_z
    return p_aabb;
}

float* CVXC_Voxelizer::getTargetIndexInAABB(float* tri_aabb, float start_position[3], float unit_voxel[3]){

    float* indices = new float[6];
    for(int i=0; i<3; i++){
        indices[i] = floor((tri_aabb[i]-voxel_origin[i])/unit_voxel[i]) - 1;
        if(indices[i] < 0 ) indices[i] = 0;

        indices[i+3] = ceil((tri_aabb[i+3]-voxel_origin[i])/unit_voxel[i]) + 1;
        if(indices[i+3] > voxel_num[i]) indices[i+3] = voxel_num[i];
    }
    return indices;
}


//
//  is_collision.cpp
//  Voxelizer
//
//  Created by atsmsmr on 2015/10/28.
//  Copyright (c) 2015 Atsushi Masumori. All rights reserved.
//
// This code was written based on an algorithm on "Fast 3D Triangle-Box Overlap Testing" by Tomas Akenine-M¨oller
// http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/tribox_tam.pdf
//

#define X 0
#define Y 1
#define Z 2


CVXC_VoxelizerUtil::CVXC_VoxelizerUtil(){
    v0[0] = 0;
    v0[1] = 0;
    v0[2] = 0;
}

CVXC_VoxelizerUtil::~CVXC_VoxelizerUtil(){}


void CVXC_VoxelizerUtil::CROSS(float* dest,float v1[3],float v2[3]){

    dest[0] = v1[1]*v2[2]-v1[2]*v2[1];
    dest[1] = v1[2]*v2[0]-v1[0]*v2[2];
    dest[2] = v1[0]*v2[1]-v1[1]*v2[0];

}


float CVXC_VoxelizerUtil::DOT(float v1[3],float v2[3]){
    return v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2];
}


void CVXC_VoxelizerUtil::SUB(float* dest,float v1[3],float v2[3]){
    dest[0] = v1[0]-v2[0];
    dest[1] = v1[1]-v2[1];
    dest[2] = v1[2]-v2[2];
}


bool CVXC_VoxelizerUtil::planeBoxOverlap(float normal[3], float vert[3], float maxbox[3]){

    int q;
    float vmin[3],vmax[3],v;

    for(q=X;q<=Z;q++){

        v=vert[q];
        if(normal[q]>0.0f){

            vmin[q]=-maxbox[q] - v;
            vmax[q]= maxbox[q] - v;

        }else{

            vmin[q]= maxbox[q] - v;
            vmax[q]=-maxbox[q] - v;

        }
    }

    if(this->DOT(normal,vmin)>0.0f) return 0;
    if(this->DOT(normal,vmax)>=0.0f) return 1;

    return 0;

}


/*======================== X-tests ========================*/

bool CVXC_VoxelizerUtil::AXISTEST_X01(float a, float b, float fa, float fb){

    p0 = a*v0[Y] - b*v0[Z];
    p2 = a*v2[Y] - b*v2[Z];

    if(p0<p2){

        vmin=p0; vmax=p2;

    }else{

        vmin=p2; vmax=p0;

    }

    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];

    if(vmin>rad || vmax<-rad){
        return 0;
    }

    return 1;
}



bool CVXC_VoxelizerUtil::AXISTEST_X2(float a, float b, float fa, float fb){

    p0 = a*v0[Y] - b*v0[Z];
    p1 = a*v1[Y] - b*v1[Z];

    if(p0<p1) {vmin=p0; vmax=p1;} else {vmin=p1; vmax=p0;}

    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];

    if(vmin>rad || vmax<-rad) return 0;

    return 1;
}


/*======================== Y-tests ========================*/

bool CVXC_VoxelizerUtil::AXISTEST_Y02(float a, float b, float fa, float fb){

    p0 = -a*v0[X] + b*v0[Z];
    p2 = -a*v2[X] + b*v2[Z];

    if(p0<p2) {vmin=p0; vmax=p2;} else {vmin=p2; vmax=p0;}

    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];

    if(vmin>rad || vmax<-rad) return 0;

    return 1;
}


bool CVXC_VoxelizerUtil::AXISTEST_Y1(float a, float b, float fa, float fb){

    p0 = -a*v0[X] + b*v0[Z];
    p1 = -a*v1[X] + b*v1[Z];

    if(p0<p1) {vmin=p0; vmax=p1;} else {vmin=p1; vmax=p0;}

    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];

    if(vmin>rad || vmax<-rad) return 0;
    return 1;
}

/*======================== Z-tests ========================*/



bool CVXC_VoxelizerUtil::AXISTEST_Z12(float a, float b, float fa, float fb){
    p1 = a*v1[X] - b*v1[Y];
    p2 = a*v2[X] - b*v2[Y];

    if(p2<p1) {vmin=p2; vmax=p1;} else {vmin=p1; vmax=p2;}

    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];

    if(vmin>rad || vmax<-rad) return 0;

    return 1;
}


bool CVXC_VoxelizerUtil::AXISTEST_Z0(float a, float b, float fa, float fb){

    p0 = a*v0[X] - b*v0[Y];
    p1 = a*v1[X] - b*v1[Y];

    if(p0<p1){
        vmin=p0; vmax=p1;
    }else{
        vmin=p1; vmax=p0;
    }

    rad = fa * this->boxhalfsize[X] + fb * this->boxhalfsize[Y];

    if(vmin>rad || vmax<-rad) return 0;

    return 1;
}


bool CVXC_VoxelizerUtil::is_collision(float boxcenter[3],float* unit_voxel,float triverts[3][3])
{
    /*    use separating axis theorem to test overlap between triangle and box */
    /*    need to test for overlap in these directions: */
    /*    1) the {x,y,z}-directions (actually, since we use the AABB of the triangle */
    /*       we do not even need to test these) */
    /*    2) normal of the triangle */
    /*    3) crossproduct(edge from tri, {x,y,z}-directin) */
    /*       this gives 3x3=9 more tests */


    /* This is the fastest branch on Sun */
    /* move everything so that the boxcenter is in (0,0,0) */


    this->boxhalfsize[0] = unit_voxel[0]*0.5f;
    this->boxhalfsize[1] = unit_voxel[1]*0.5f;
    this->boxhalfsize[2] = unit_voxel[2]*0.5f;

    SUB(v0,triverts[0],boxcenter);
    SUB(v1,triverts[1],boxcenter);
    SUB(v2,triverts[2],boxcenter);

    /* compute triangle edges */
    SUB(e0,v1,v0);      /* tri edge 0 */
    SUB(e1,v2,v1);      /* tri edge 1 */
    SUB(e2,v0,v2);      /* tri edge 2 */

    /* Bullet 3:  */
    /*  test the 9 tests first (this was faster) */

    fex = fabsf(e0[X]);
    fey = fabsf(e0[Y]);
    fez = fabsf(e0[Z]);

    if(!AXISTEST_X01(e0[Z], e0[Y], fez, fey)) return 0;
    else if(!AXISTEST_Y02(e0[Z], e0[X], fez, fex)) return 0;
    else if(!AXISTEST_Z12(e0[Y], e0[X], fey, fex)) return 0;


    fex = fabsf(e1[X]);
    fey = fabsf(e1[Y]);
    fez = fabsf(e1[Z]);

    if(!AXISTEST_X01(e1[Z], e1[Y], fez, fey)) return 0;
    else if(!AXISTEST_Y02(e1[Z], e1[X], fez, fex)) return 0;
    else if(!AXISTEST_Z0(e1[Y], e1[X], fey, fex)) return 0;


    fex = fabsf(e2[X]);
    fey = fabsf(e2[Y]);
    fez = fabsf(e2[Z]);

    if(!AXISTEST_X2(e2[Z], e2[Y], fez, fey)) return 0;
    else if(!AXISTEST_Y1(e2[Z], e2[X], fez, fex)) return 0;
    else if(!AXISTEST_Z12(e2[Y], e2[X], fey, fex)) return 0;


    /* Bullet 1: */
    /*  first test overlap in the {x,y,z}-directions */
    /*  find min, max of the triangle each direction, and test for overlap in */
    /*  that direction -- this is equivalent to testing a minimal AABB around */
    /*  the triangle against the AABB */


    /* test in X-direction */
    vmax = fmax(fmax(v0[X],v1[X]),v2[X]);
    vmin = fmin(fmin(v0[X],v1[X]),v2[X]);
    if(vmin>boxhalfsize[X] || vmax<-boxhalfsize[X]) return 0;

    /* test in Y-direction */
    vmax = fmax(fmax(v0[Y],v1[Y]),v2[Y]);
    vmin = fmin(fmin(v0[Y],v1[Y]),v2[Y]);
    if(vmin>boxhalfsize[Y] || vmax<-boxhalfsize[Y]) return 0;

    /* test in Z-direction */
    vmax = fmax(fmax(v0[Z],v1[Z]),v2[Z]);
    vmin = fmin(fmin(v0[Z],v1[Z]),v2[Z]);
    if(vmin>boxhalfsize[Z] || vmax<-boxhalfsize[Z]) return 0;

    /* Bullet 2: */
    /*  test if the box intersects the plane of the triangle */
    /*  compute plane equation of triangle: normal*x+d=0 */
    CROSS(normal,e0,e1);

    if(!planeBoxOverlap(normal,v0,boxhalfsize)) return 0;	// -NJMP-

    return 1;   /* box and triangle overlaps */
}



