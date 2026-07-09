#include <iostream>
#include "C3DShape.h"
#include "commontypes.h"
#include "SphereDrawer.h"
#include "BoxDrawer.h"
#include "CylinderDrawer.h"
#include "ConeDrawer.h"
#include "LineDrawer.h"
#include "CapsuleDrawer.h"

bool C3DShape::m_first = true;

C3DShape::C3DShape() {

    if (m_first) {
          SphereDrawerInit();
          BoxDrawerInit();
          CylinderDrawerInit();
          ConeDrawerInit(true);
          LineDrawerInit();
          CapsuleDrawerInit();
          m_first = false;
    }
}

void Sphere::Draw(SRT srt, Color col) {
     srt.scale = Vector3(m_radius, m_radius, m_radius);
     SphereDrawerDraw(srt, col);
}

void Sphere::Draw(Matrix4x4 mtx, Color col) {
     Matrix4x4 scale = Matrix4x4::CreateScale(m_radius, m_radius, m_radius);
     mtx = scale * mtx;
     SphereDrawerDraw(mtx, col);
}


void Cone::Draw(SRT srt,Color col) {
      srt.scale = Vector3(m_radius, m_height, m_radius);
      ConeDrawerDraw(srt, col);
}

void Cone::Draw(Matrix4x4 mtx, Color col) {
     Matrix4x4 scale = Matrix4x4::CreateScale(m_radius, m_height, m_radius);
     mtx = scale * mtx;
     ConeDrawerDraw(mtx, col);
}

void Box::Draw(SRT srt,Color col) {
     srt.scale = Vector3(m_width, m_height, m_depth);
     BoxDrawerDraw(srt, col);
}

void Box::Draw(Matrix4x4 mtx, Color col) {
     Matrix4x4 scale = Matrix4x4::CreateScale(m_width, m_height, m_depth);
     mtx = scale * mtx;
     BoxDrawerDraw(mtx, col);
}

void Cylinder::Draw(SRT srt, Color col) {
     srt.scale = Vector3(m_radius, m_height, m_radius);
     CylinderDrawerDraw(srt, col);
}

void Cylinder::Draw(Matrix4x4 mtx, Color col) {
     Matrix4x4 scale = Matrix4x4::CreateScale(m_radius, m_height, m_radius);
     mtx = scale * mtx;
     CylinderDrawerDraw(mtx, col);
}

void Segment::Draw(Matrix4x4 mtx, Color col)
{
	Vector3 start = Vector3::Transform(m_start, mtx);
    Vector3 end = Vector3::Transform(m_end, mtx);

    Vector3 dir = end-start;
	SetLineWidth(m_width);  
    LineDrawerDraw(m_length,start,dir,col);
}

void Segment::Draw(SRT srt, Color col)
{
    SetLineWidth(m_width);
    LineDrawerDraw(m_length, m_start, m_direction, col);
}

void Capsule::Draw(SRT srt, Color col) {
    srt.scale = Vector3(m_radius, m_height, m_radius);
    CylinderDrawerDraw(srt, col);
}

void Capsule::Draw(Matrix4x4 mtx, Color col) 
{
    Matrix4x4 scale = Matrix4x4::CreateScale(m_radius, m_height, m_radius);
    Matrix4x4 cmtx = scale * mtx;
    CylinderDrawerDraw(cmtx, col);

    scale = Matrix4x4::CreateScale(m_radius, m_radius, m_radius);
    Matrix4x4 smtx = scale * mtx;
    SphereDrawerDraw(smtx, col);

    smtx = scale * Matrix4x4::CreateTranslation(0, m_height, 0) * mtx;
    SphereDrawerDraw(smtx, col);
}
