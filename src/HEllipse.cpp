// HEllipse.cpp
// Copyright (c) 2009, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.
#include "stdafx.h"

#include "HEllipse.h"
#include "PropertyDouble.h"
#include "PropertyLength.h"
#include "PropertyChoice.h"
#include "PropertyVertex.h"
#include "HLine.h"
#include "HILine.h"
#include "HArc.h"
#include "HPoint.h"
#include "Gripper.h"

HEllipse::HEllipse(const HEllipse &e){
	operator=(e);
}

HEllipse::HEllipse(const gp_Elips &e, const HeeksColor* col):color(*col){
	C = e.Location();
	m_start = 0;
	m_end = 2*M_PI;
	SetEllipse(e);
}

HEllipse::HEllipse(const gp_Elips &e, double start, double end, const HeeksColor* col):color(*col){
	m_start = start; m_end = end;
	C = e.Location();
	SetEllipse(e);
}

HEllipse::~HEllipse(){
}

const HEllipse& HEllipse::operator=(const HEllipse &e){
	HeeksObj::operator =(e);
	m_start = e.m_start; m_end = e.m_end;
	color = e.color;
	C = e.C;
	SetEllipse(e.GetEllipse());
	return *this;
}


const wxBitmap &HEllipse::GetIcon()
{
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(wxGetApp().GetResFolder() + _T("/icons/ellipse.png")));
	return *icon;
}

//segments - number of segments per full revolution!
void HEllipse::GetSegments(void(*callbackfunc)(const double *p), double pixels_per_mm, bool want_start_point)const
{
	gp_Dir x_axis = m_xdir;
	gp_Dir y_axis = m_zdir ^ m_xdir;
	gp_Pnt centre = C;

	double radius = m_majr;
	double min_radius = m_minr;

	double ratio = min_radius/radius;

	gp_Pnt zp(0,0,0);
	gp_Dir up(0,0,1);

	double end_angle = m_end;
	double start_angle = m_start;

	double d_angle = end_angle - start_angle;

	if(fabs(d_angle) < wxGetApp().m_geom_tol)
		d_angle = 2*M_PI;

	if(d_angle < 0)
		d_angle += 2*M_PI;

	int segments = (int)(fabs(pixels_per_mm * radius * d_angle / (2*M_PI) + 1));
	if(segments > 1000)
		segments = 1000;

    double theta = d_angle / (double)segments;
	while(theta>1.0){segments*=2;theta = d_angle / (double)segments;}
    double tangetial_factor = tan(theta);
    double radial_factor = 1 - cos(theta);

    double x = radius * cos(start_angle);
    double y = radius * sin(start_angle);

	double pp[3];

   for(int i = 0; i < segments + 1; i++)
    {
		gp_Pnt p = centre.XYZ() + x * x_axis.XYZ() + y * y_axis.XYZ()  * ratio;
		extract(p, pp);
		(*callbackfunc)(pp);

        double tx = -y;
        double ty = x;

        x += tx * tangetial_factor;
        y += ty * tangetial_factor;

        double rx = - x;
        double ry = - y;

        x += rx * radial_factor;
        y += ry * radial_factor;
    }
}

static void glVertexFunction(const double *p){glVertex3d(p[0], p[1], p[2]);}

void HEllipse::glCommands(bool select, bool marked, bool no_color){
	if(!no_color){
		wxGetApp().glColorEnsuringContrast(color);
	}
	GLfloat save_depth_range[2];
	if(marked){
		glGetFloatv(GL_DEPTH_RANGE, save_depth_range);
		glDepthRange(0, 0);
		glLineWidth(2);
	}

	glBegin(GL_LINE_STRIP);
	GetSegments(glVertexFunction, wxGetApp().GetPixelScale());
	glEnd();

	if(marked){
		glLineWidth(1);
		glDepthRange(save_depth_range[0], save_depth_range[1]);
	}
}

HeeksObj *HEllipse::MakeACopy(void)const{
		HEllipse *new_object = new HEllipse(*this);
		return new_object;
}

void HEllipse::ModifyByMatrix(const double* m){
	gp_Trsf mat = make_matrix(m);
	SetEllipse(GetEllipse().Transformed(mat));
}

void HEllipse::GetBox(CBox &box){
	gp_Elips m_ellipse = GetEllipse();
	gp_Dir x_axis = m_ellipse.XAxis().Direction();
	gp_Dir y_axis = m_ellipse.YAxis().Direction();
	gp_XYZ c = m_ellipse.Location().XYZ();
	gp_XYZ x = x_axis.XYZ() * m_ellipse.MajorRadius();
	gp_XYZ y = y_axis.XYZ() * m_ellipse.MinorRadius();

	gp_Pnt p[4];
	p[0] = gp_Pnt(c - x - y);
	p[1] = gp_Pnt(c + x - y);
	p[2] = gp_Pnt(c + x + y);
	p[3] = gp_Pnt(c - x + y);

	for(int i = 0; i<4; i++)
	{
		double pp[3];
		extract(p[i], pp);
		box.Insert(pp);
	}
}

void HEllipse::GetGripperPositions(std::list<GripData> *list, bool just_for_endof){
	gp_Elips m_ellipse = GetEllipse();
	if(!just_for_endof)
	{
		gp_Dir x_axis = m_ellipse.XAxis().Direction();
		gp_Dir y_axis = m_ellipse.YAxis().Direction();
		gp_XYZ c = m_ellipse.Location().XYZ();
		double maj_r = m_ellipse.MajorRadius();
		double min_r = m_ellipse.MinorRadius();
		gp_Pnt maj_s(c + x_axis.XYZ() * maj_r);
		gp_Pnt min_s(c + y_axis.XYZ() * min_r);
		gp_Pnt rot(c+x_axis.XYZ() * maj_r * -1);

		list->push_back(GripData(GripperTypeStretch,maj_s.X(),maj_s.Y(),maj_s.Z(),&m_majr));

		list->push_back(GripData(GripperTypeStretch,min_s.X(),min_s.Y(),min_s.Z(),&m_minr));

		list->push_back(GripData(GripperTypeStretch,c.X(),c.Y(),c.Z(),(void*)&C));

	    list->push_back(GripData(GripperTypeRotate,rot.X(),rot.Y(),rot.Z(),NULL));

	}
}

class PropertyEllipseRotation : public Property
{
public:
	PropertyEllipseRotation(HeeksObj* object) :Property(object, _("rotation")){}
	int get_property_type(){ return DoublePropertyType; }
	Property *MakeACopy(void)const{ return new PropertyEllipseRotation(*this); }
	void Set(double value){
		((HEllipse*)m_object)->SetRotation(value);
		m_object->OnApplyProperties();
	}
	double GetDouble()const{ return ((HEllipse*)m_object)->GetRotation(); }
};

void HEllipse::SetRotation(double value)
{
	gp_Dir up(0, 0, 1);
	gp_Pnt zp(0,0,0);
    double rot = GetRotation();

	gp_Ax2 a(C,m_zdir,m_xdir);
	a.Rotate(gp_Ax1(C,up),value-rot);
	m_zdir = a.Direction();
	m_xdir = a.XDirection();
}

double HEllipse::GetRotation()const
{
	return GetEllipseRotation(GetEllipse());
}

void HEllipse::GetProperties(std::list<Property *> *list){
	list->push_back(PropertyPnt(this, _("centre"), &C));
	list->push_back(PropertyDir(this, _("x axis"), &m_xdir));
	list->push_back(PropertyDir(this, _("z axis"), &m_zdir));
	list->push_back(new PropertyLength(this, _("major radius"), &m_majr));
	list->push_back(new PropertyLength(this, _("minor radius"), &m_minr));
	list->push_back(new PropertyEllipseRotation(this));
	list->push_back(new PropertyDouble(this, _("start angle"), &m_start));
	list->push_back(new PropertyDouble(this, _("end angle"), &m_end));
	HeeksObj::GetProperties(list);
}

bool HEllipse::FindNearPoint(const double* ray_start, const double* ray_direction, double *point){
	gp_Lin ray(make_point(ray_start), make_vector(ray_direction));
	std::list< gp_Pnt > rl;
	ClosestPointsLineAndEllipse(ray, GetEllipse(), rl);
	if(rl.size()>0)
	{
		extract(rl.front(), point);
		return true;
	}

	return false;
}

bool HEllipse::FindPossTangentPoint(const double* ray_start, const double* ray_direction, double *point){
	// any point on this ellipse is a possible tangent point
	return FindNearPoint(ray_start, ray_direction, point);
}

bool HEllipse::Stretch(const double *p, const double* shift, void* data){

	//TODO:
        // 1. When the major and minor axis swap, the unused handle switches sides.
	// 2. The handle switches to the other radius if you go past M_PI/4

	gp_Pnt vp = make_point(p);
	gp_Pnt zp(0,0,0);
	gp_Dir up(0, 0, 1);
	gp_Vec vshift = make_vector(shift);

	double rot = GetRotation();

	gp_Dir x_axis = m_xdir;
	gp_Dir y_axis = m_zdir ^ m_xdir;
	gp_Pnt c = C;
	double maj_r = m_majr;
	double min_r = m_minr;
	gp_Pnt maj_s(c.XYZ() + x_axis.XYZ() * maj_r);
	gp_Pnt min_s(c.XYZ() + y_axis.XYZ() * min_r);

	gp_Pnt np = vp.XYZ() + vshift.XYZ();
#if 0
	// these cause compiler warnings, so I have commented them out
    double d = c.Distance(np);
    double f = DistanceToFoci(np,m_ellipse)/2;
#endif
	if(data == &C){
		C = np;
	}
    else if(data == &m_majr || data == &m_minr)
	{
		//We have to rotate the incoming vector to be in our coordinate system
		gp_Pnt cir = np.XYZ() - c.XYZ();
		cir.Rotate(gp_Ax1(zp,up),-rot);

		//This is shockingly simple
		if( data == (void*)1)
		{
			double nradius = 1/sqrt((1-(1/min_r)*(1/min_r)*cir.X()*cir.X()) / cir.Y() / cir.Y());
			if(nradius > 1 / wxGetApp().m_geom_tol || nradius != nradius)
				nradius = 1 / wxGetApp().m_geom_tol;

			if(nradius > min_r)
				m_majr = nradius;
			else
			{
				m_majr = min_r;
				m_minr = nradius;
				SetRotation(GetRotation()-M_PI/2);
				m_start += M_PI/2;
				m_end += M_PI/2;
			}
		}
		else
		{
			double nradius = 1/sqrt((1-(1/maj_r)*(1/maj_r)*cir.Y()*cir.Y()) / cir.X() / cir.X());
			if(nradius > 1 / wxGetApp().m_geom_tol || nradius != nradius)
				nradius = 1 / wxGetApp().m_geom_tol;
			if(nradius < maj_r)
				m_minr = nradius;
			else
			{
				m_minr = maj_r;
				m_majr = nradius;
				SetRotation(GetRotation()+M_PI/2);
				m_start-=M_PI/2;
				m_end-=M_PI/2;
			}
		}
	}
	return false;
}

bool HEllipse::GetCentrePoint(double* pos)
{
	extract(C, pos);
	return true;
}

void HEllipse::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element;
	element = new TiXmlElement( "Ellipse" );
	root->LinkEndChild( element );
	element->SetAttribute("col", color.COLORREF_color());
	element->SetDoubleAttribute("maj", m_majr);
	element->SetDoubleAttribute("min", m_minr);
	element->SetDoubleAttribute("rot", GetRotation());
	element->SetDoubleAttribute("cx", C.X());
	element->SetDoubleAttribute("cy", C.Y());
	element->SetDoubleAttribute("cz", C.Z());
	gp_Dir D = m_zdir;
	element->SetDoubleAttribute("ax", D.X());
	element->SetDoubleAttribute("ay", D.Y());
	element->SetDoubleAttribute("az", D.Z());
	element->SetDoubleAttribute("start", m_start);
	element->SetDoubleAttribute("end", m_end);
	WriteBaseXML(element);
}

// static member function
HeeksObj* HEllipse::ReadFromXMLElement(TiXmlElement* pElem)
{
	double axis[3];
	double maj = 0.0;
	double min = 0.0;
	double rot = 0;
	double start=0;
	double end=2*M_PI;
	HeeksColor c;
	gp_Pnt p0(1,0,0), p1(0,1,0), centre(0,0,0);

	int att_col;
	double x;
	if(pElem->Attribute("col", &att_col))c = HeeksColor((long)att_col);
	pElem->Attribute("maj", &maj);
	pElem->Attribute("min", &min);
	pElem->Attribute("rot", &rot);
	pElem->Attribute("start", &start);
	pElem->Attribute("end", &end);
	if(pElem->Attribute("ax", &x))axis[0] = x;
	if(pElem->Attribute("ay", &x))axis[1] = x;
	if(pElem->Attribute("az", &x))axis[2] = x;
	if(pElem->Attribute("cx", &x))centre.SetX(x);
	if(pElem->Attribute("cy", &x))centre.SetY(x);
	if(pElem->Attribute("cz", &x))centre.SetZ(x);

	else
	{
		// try the version where the centre point was a child
		for(TiXmlElement* pElem2 = TiXmlHandle(pElem).FirstChildElement().Element(); pElem2;	pElem2 = pElem2->NextSiblingElement())
		{
			HeeksObj* object = wxGetApp().ReadXMLElement(pElem2);
			if(object->GetType() == PointType)
			{
				centre = ((HPoint*)object)->m_p;
				delete object;
				break;
			}
			delete object;
		}
	}

	gp_Elips ellipse(gp_Ax2(centre, gp_Dir(make_vector(axis))), maj,min);

	HEllipse* new_object = new HEllipse(ellipse,start,end,&c);
	new_object->SetRotation(rot);
	new_object->ReadBaseXML(pElem);

	return new_object;
}

void HEllipse::SetEllipse(gp_Elips e)
{
	C = e.Location();
	m_zdir = e.Axis().Direction();
	m_xdir = e.XAxis().Direction();
	m_majr = e.MajorRadius();
	m_minr = e.MinorRadius();
}

gp_Elips HEllipse::GetEllipse() const
{
	return gp_Elips(gp_Ax2(C,m_zdir,m_xdir),m_majr,m_minr);
}

int HEllipse::Intersects(const HeeksObj *object, std::list< double > *rl)const
{
	int numi = 0;
	gp_Elips m_ellipse = GetEllipse();

	switch(object->GetType())
	{
    case SketchType:
        return( ((CSketch *)object)->Intersects( this, rl ));

	case LineType:
		{
			std::list<gp_Pnt> plist;
			intersect(((HLine*)object)->GetLine(), m_ellipse, plist);
			for(std::list<gp_Pnt>::iterator It = plist.begin(); It != plist.end(); It++)
			{
				gp_Pnt& pnt = *It;
				if(((HLine*)object)->Intersects(pnt))
				{
					if(rl)add_pnt_to_doubles(pnt, *rl);
					numi++;
				}
			}
		}
		break;

	case ILineType:
		{
			std::list<gp_Pnt> plist;
			intersect(((HILine*)object)->GetLine(), m_ellipse, plist);
			if(rl)convert_pnts_to_doubles(plist, *rl);
			numi += plist.size();
		}
		break;

/*	case ArcType:
		{
			std::list<gp_Pnt> plist;
			intersect(m_ellipse, ((HArc*)object)->m_circle, plist);
			for(std::list<gp_Pnt>::iterator It = plist.begin(); It != plist.end(); It++)
			{
				gp_Pnt& pnt = *It;
				if(((HArc*)object)->Intersects(pnt))
				{
					if(rl)add_pnt_to_doubles(pnt, *rl);
					numi++;
				}
			}
		}
		break;

	case CircleType:
		{
			std::list<gp_Pnt> plist;
			intersect(m_ellipse, ((HEllipse*)object)->m_circle, plist);
			if(rl)convert_pnts_to_doubles(plist, *rl);
			numi += plist.size();
		}
		break; */
	}

	return numi;
}





