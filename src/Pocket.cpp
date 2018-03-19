// Pocket.cpp
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"
#include "Pocket.h"
#include "HeeksConfig.h"
#include "ProgramCanvas.h"
#include "Program.h"
#include "HeeksObj.h"
#include "PropertyInt.h"
#include "PropertyDouble.h"
#include "PropertyLength.h"
#include "PropertyString.h"
#include "PropertyChoice.h"
#include "PropertyVertex.h"
#include "PropertyCheck.h"
#include "tinyxml.h"
#include "CTool.h"
#include "CNCPoint.h"
#include "PocketDlg.h"
#include "HSpline.h"
#include "HCircle.h"
#include "HArc.h"

#include <sstream>

// static
double CPocket::max_deviation_for_spline_to_arc = 0.1;

void CPocket::set_initial_values(int tool_number)
{
    if (tool_number > 0)
    {
        CTool *pTool = CTool::Find(tool_number);
        if (pTool != NULL)
        {
            m_step_over = pTool->CuttingRadius() * 3.0 / 5.0;
        }
    }
}

static void on_set_entry_move(int value, HeeksObj* object, bool from_undo_redo)
{
	((CPocket*)object)->m_entry_move = (CPocket::eEntryStyle) value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_step_over(double value, HeeksObj* object)
{
	((CPocket*)object)->m_step_over = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_material_allowance(double value, HeeksObj* object)
{
	((CPocket*)object)->m_material_allowance = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_starting_place(int value, HeeksObj* object, bool from_undo_redo)
{
	((CPocket*)object)->m_starting_place = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_keep_tool_down(bool value, HeeksObj* object)
{
	((CPocket*)object)->m_keep_tool_down_if_poss = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_use_zig_zag(bool value, HeeksObj* object)
{
	((CPocket*)object)->m_use_zig_zag = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_zig_angle(double value, HeeksObj* object)
{
	((CPocket*)object)->m_zig_angle = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_zig_uni(bool value, HeeksObj* object)
{
	((CPocket*)object)->m_zig_unidirectional = value;
	((CPocket*)object)->WriteDefaultValues();
}

static void on_set_cut_mode(int value, HeeksObj* object, bool from_undo_redo)
{
	((CPocket*)object)->m_cut_mode = (CPocket::eCutMode)value;
	((CPocket*)object)->WriteDefaultValues();
}

void CPocket::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = new TiXmlElement( "params" );
	wxGetApp().LinkXMLEndChild( root,  element );
	element->SetDoubleAttribute( "step", m_step_over);
	element->SetAttribute( "cut_mode", m_cut_mode);
	element->SetDoubleAttribute( "mat", m_material_allowance);
	element->SetAttribute( "from_center", m_starting_place);
	element->SetAttribute( "keep_tool_down", m_keep_tool_down_if_poss ? 1:0);
	element->SetAttribute( "use_zig_zag", m_use_zig_zag ? 1:0);
	element->SetDoubleAttribute( "zig_angle", m_zig_angle);
	element->SetAttribute( "zig_unidirectional", m_zig_unidirectional ? 1:0);
	element->SetAttribute( "entry_move", (int) m_entry_move);
}

void CPocket::ReadParamsFromXMLElement(TiXmlElement* pElem)
{
	pElem->Attribute("step", &m_step_over);
	int int_for_enum;
	if(pElem->Attribute("cut_mode", &int_for_enum))m_cut_mode = (eCutMode)int_for_enum;
	pElem->Attribute("mat", &m_material_allowance);
	pElem->Attribute("from_center", &m_starting_place);
	int int_for_bool = false;
	pElem->Attribute("keep_tool_down", &int_for_bool);
	m_keep_tool_down_if_poss = (int_for_bool != 0);
	pElem->Attribute("use_zig_zag", &int_for_bool);
	m_use_zig_zag = (int_for_bool != 0);
	pElem->Attribute("zig_angle", &m_zig_angle);
	pElem->Attribute("zig_unidirectional", &int_for_bool);
	m_zig_unidirectional = (int_for_bool != 0);
	int int_for_entry_move = (int) ePlunge;
	pElem->Attribute("entry_move", &int_for_entry_move);
	m_entry_move = (eEntryStyle) int_for_entry_move;
}


static wxString WriteSketchDefn(HeeksObj* sketch)
{
#ifdef UNICODE
	std::wostringstream gcode;
#else
    std::ostringstream gcode;
#endif
    gcode.imbue(std::locale("C"));
	gcode << std::setprecision(10);

	bool started = false;

	double prev_e[3];

	std::list<HeeksObj*> new_spans;
	for(HeeksObj* span = sketch->GetFirstChild(); span; span = sketch->GetNextChild())
	{
		if(span->GetType() == SplineType)
		{
			((HSpline*)span)->ToBiarcs(new_spans, CPocket::max_deviation_for_spline_to_arc);
		}
		else
		{
			new_spans.push_back(span->MakeACopy());
		}
	}

	for(std::list<HeeksObj*>::iterator It = new_spans.begin(); It != new_spans.end(); It++)
	{
		HeeksObj* span_object = *It;

		double s[3] = {0, 0, 0};
		double e[3] = {0, 0, 0};
		double c[3] = {0, 0, 0};

		if(span_object){
			int type = span_object->GetType();

			if(type == LineType || type == ArcType)
			{
				span_object->GetStartPoint(s);
				CNCPoint start(s);

				if(started && (fabs(s[0] - prev_e[0]) > 0.0001 || fabs(s[1] - prev_e[1]) > 0.0001))
				{
					gcode << _T("a.append(c)\n");
					started = false;
				}

				if(!started)
				{
					gcode << _T("c = area.Curve()\n");
					gcode << _T("c.append(area.Vertex(0, area.Point(") << start.X(true) << _T(", ") << start.Y(true) << _T("), area.Point(0, 0)))\n");
					started = true;
				}
				span_object->GetEndPoint(e);
				CNCPoint end(e);

				if(type == LineType)
				{
					gcode << _T("c.append(area.Vertex(0, area.Point(") << end.X(true) << _T(", ") << end.Y(true) << _T("), area.Point(0, 0)))\n");
				}
				else if(type == ArcType)
				{
					span_object->GetCentrePoint(c);
					CNCPoint centre(c);

					double pos[3];
					extract(((HArc*)span_object)->m_axis.Direction(), pos);
					int span_type = (pos[2] >=0) ? 1:-1;
					gcode << _T("c.append(area.Vertex(") << span_type << _T(", area.Point(") << end.X(true) << _T(", ") << end.Y(true);
					gcode << _T("), area.Point(") << centre.X(true) << _T(", ") << centre.Y(true) << _T(")))\n");
				}
				memcpy(prev_e, e, 3*sizeof(double));
			} // End if - then
			else
			{
				if (type == CircleType)
				{
					if(started)
					{
						gcode << _T("a.append(c)\n");
						started = false;
					}

					std::list< std::pair<int, gp_Pnt > > points;
					span_object->GetCentrePoint(c);

					// Setup the four arcs that will make up the circle using UNadjusted
					// coordinates first so that the offsets align with the X and Y axes.
					double radius = ((HCircle*)span_object)->m_radius;

					points.push_back( std::make_pair(0, gp_Pnt( c[0], c[1] + radius, c[2] )) ); // north
					points.push_back( std::make_pair(-1, gp_Pnt( c[0] + radius, c[1], c[2] )) ); // east
					points.push_back( std::make_pair(-1, gp_Pnt( c[0], c[1] - radius, c[2] )) ); // south
					points.push_back( std::make_pair(-1, gp_Pnt( c[0] - radius, c[1], c[2] )) ); // west
					points.push_back( std::make_pair(-1, gp_Pnt( c[0], c[1] + radius, c[2] )) ); // north

					CNCPoint centre(c);

					gcode << _T("c = area.Curve()\n");
					for (std::list< std::pair<int, gp_Pnt > >::iterator l_itPoint = points.begin(); l_itPoint != points.end(); l_itPoint++)
					{
						CNCPoint pnt( l_itPoint->second );

						gcode << _T("c.append(area.Vertex(") << l_itPoint->first << _T(", area.Point(");
						gcode << pnt.X(true) << (_T(", ")) << pnt.Y(true);
						gcode << _T("), area.Point(") << centre.X(true) << _T(", ") << centre.Y(true) << _T(")))\n");
					} // End for
					gcode << _T("a.append(c)\n");
				}
			} // End if - else
		}
	}

	if(started)
	{
		gcode << _T("a.append(c)\n");
		started = false;
	}

	// delete the spans made
	for(std::list<HeeksObj*>::iterator It = new_spans.begin(); It != new_spans.end(); It++)
	{
		HeeksObj* span = *It;
		delete span;
	}

	gcode << _T("\n");
	return(wxString(gcode.str().c_str()));
}

const wxBitmap &CPocket::GetIcon()
{
	if(!m_active)return GetInactiveIcon();
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(wxGetApp().GetResFolder() + _T("/icons/pocket.png")));
	return *icon;
}

void CPocket::WritePocketPython(Python &python)
{
	// start - assume we are at a suitable clearance height

	// make a parameter of area_funcs.pocket() eventually
	// 0..plunge, 1..ramp, 2..helical
	python << _T("entry_style = ") <<  m_entry_move << _T("\n");

	// Pocket the area
	python << _T("area_funcs.pocket(a, tool_diameter/2, ");
	python << m_material_allowance / wxGetApp().m_program->m_units;
	python << _T(", ") << m_step_over / wxGetApp().m_program->m_units;
	python << _T(", depthparams, ");
	python << m_starting_place;
	python << (m_keep_tool_down_if_poss ? _T(", True") : _T(", False"));
	python << (m_use_zig_zag ? _T(", True") : _T(", False"));
	python << _T(", ") << m_zig_angle;
	python << _T(",") << (m_zig_unidirectional ? _T("True") : _T("False"));
	python << _T(", None"); // start point
	python << _T(",") << ((m_cut_mode == CPocket::eClimb) ? _T("'climb'") : _T("'conventional'"));
	python << _T(")\n");

	// rapid back up to clearance plane
	python << _T("rapid(z = depthparams.clearance_height)\n");
}

Python CPocket::AppendTextToProgram()
{
	Python python;

	CTool *pTool = CTool::Find( m_tool_number );
	if (pTool == NULL)
	{
		wxMessageBox(_T("Cannot generate GCode for pocket without a tool assigned"));
		return python;
	} // End if - then


	python << CSketchOp::AppendTextToProgram();

	HeeksObj* object = wxGetApp().GetIDObject(SketchType, m_sketch);

	if(object == NULL) {
		wxMessageBox(wxString::Format(_("Pocket operation - Sketch doesn't exist")));
		return python;
	}

	int type = object->GetType();

	// do areas and circles first, separately
    {
		switch(type)
		{
		case CircleType:
		case AreaType:
			{
				wxGetApp().ObjectAreaString(object, python);
				WritePocketPython(python);
			}
			break;
		}
	}

	if(type == SketchType)
	{
		python << _T("a = area.Area()\n");
		python << _T("entry_moves = []\n");

		if (object->GetNumChildren() == 0){
			wxMessageBox(wxString::Format(_("Pocket operation - Sketch %d has no children"), object->GetID()));
			return python;
		}

		HeeksObj* re_ordered_sketch = NULL;
		SketchOrderType order = ((CSketch*)object)->GetSketchOrder();
		if( 	(order != SketchOrderTypeCloseCW) &&
			(order != SketchOrderTypeCloseCCW) &&
			(order != SketchOrderTypeMultipleCurves) &&
			(order != SketchOrderHasCircles))
		{
			re_ordered_sketch = object->MakeACopy();
			((CSketch*)re_ordered_sketch)->ReOrderSketch(SketchOrderTypeReOrder);
			object = re_ordered_sketch;
			order = ((CSketch*)object)->GetSketchOrder();
			if(	(order != SketchOrderTypeCloseCW) &&
				(order != SketchOrderTypeCloseCCW) &&
				(order != SketchOrderTypeMultipleCurves) &&
				(order != SketchOrderHasCircles))
			{
				switch (((CSketch*)object)->GetSketchOrder())
				{
				case SketchOrderTypeOpen:
					{
						wxMessageBox(wxString::Format(_("Pocket operation - Sketch must be a closed shape - sketch %d"), object->m_id));
						delete re_ordered_sketch;
						return python;
					}
					break;

				default:
					{
						wxMessageBox(wxString::Format(_("Pocket operation - Badly ordered sketch - sketch %d"), object->m_id));
						delete re_ordered_sketch;
						return python;
					}
					break;
				}
			}
		}

		if(object)
		{
			python << WriteSketchDefn(object);
		}

		if(re_ordered_sketch)
		{
			delete re_ordered_sketch;
		}

	} // End for

	// reorder the area, the outside curves must be made anti-clockwise and the insides clockwise
	python << _T("a.Reorder()\n");

	WritePocketPython(python);

	return python;
}

void CPocket::WriteDefaultValues()
{
	CSketchOp::WriteDefaultValues();

	HeeksConfig config;
	config.Write(_T("StepOver"), m_step_over);
	config.Write(_T("CutMode"), (int)(m_cut_mode));
	config.Write(_T("MaterialAllowance"), m_material_allowance);
	config.Write(_T("FromCenter"), m_starting_place);
	config.Write(_T("KeepToolDown"), m_keep_tool_down_if_poss);
	config.Write(_T("UseZigZag"), m_use_zig_zag);
	config.Write(_T("ZigAngle"), m_zig_angle);
	config.Write(_T("ZigUnidirectional"), m_zig_unidirectional);
	config.Write(_T("DecentStrategy"), (int)(m_entry_move));
}

void CPocket::ReadDefaultValues()
{
	CSketchOp::ReadDefaultValues();

	HeeksConfig config;
	config.Read(_T("StepOver"), &m_step_over, 1.0);
	int int_mode = m_cut_mode;
	config.Read(_T("CutMode"), &int_mode, CPocket::eConventional);
	m_cut_mode = (CPocket::eCutMode)int_mode;
	config.Read(_T("MaterialAllowance"), &m_material_allowance, 0.2);
	config.Read(_T("FromCenter"), &m_starting_place, 1);
	config.Read(_T("KeepToolDown"), &m_keep_tool_down_if_poss, true);
	config.Read(_T("UseZigZag"), &m_use_zig_zag, false);
	config.Read(_T("ZigAngle"), &m_zig_angle);
	config.Read(_T("ZigUnidirectional"), &m_zig_unidirectional, false);
	int int_for_entry_move = CPocket::ePlunge;
	config.Read(_T("DecentStrategy"), &int_for_entry_move);
	m_entry_move = (CPocket::eEntryStyle) int_for_entry_move;
}

void CPocket::glCommands(bool select, bool marked, bool no_color)
{
	CSketchOp::glCommands( select, marked, no_color );
}

void CPocket::GetProperties(std::list<Property *> *list)
{
#if 0 // to do
	int tool_type = CTool::FindToolType(this->m_tool_number);

	list->push_back(new PropertyLength(_("step over"), m_step_over, this, on_set_step_over));
	list->push_back(new PropertyLength(_("material allowance"), m_material_allowance, this, on_set_material_allowance));

	if (CTool::IsMillingToolType(tool_type)){
		std::list< wxString > choices;
		choices.push_back(_("Conventional"));
		choices.push_back(_("Climb"));
		list->push_back(new PropertyChoice(_("cut mode"), choices, m_cut_mode, this, on_set_cut_mode));
	}

	{
		std::list< wxString > choices;
		choices.push_back(_("Boundary"));
		choices.push_back(_("Center"));
		list->push_back(new PropertyChoice(_("starting_place"), choices, m_starting_place, this, on_set_starting_place));
	}
	{
		std::list< wxString > choices;
		choices.push_back(_("Plunge"));
		choices.push_back(_("Ramp"));
		choices.push_back(_("Helical"));
		list->push_back(new PropertyChoice(_("entry_move"), choices, m_entry_move, this, on_set_entry_move));
	}
	list->push_back(new PropertyCheck(_("keep tool down"), m_keep_tool_down_if_poss, this, on_set_keep_tool_down));
	list->push_back(new PropertyCheck(_("use zig zag"), m_use_zig_zag, this, on_set_use_zig_zag));
	if (m_use_zig_zag)
	{
		list->push_back(new PropertyDouble(_("zig angle"), m_zig_angle, this, on_set_zig_angle));
		list->push_back(new PropertyCheck(_("unidirectional"), m_zig_unidirectional, this, on_set_zig_uni));
	}
#endif
	CSketchOp::GetProperties(list);
}

HeeksObj *CPocket::MakeACopy(void)const
{
	return new CPocket(*this);
}

void CPocket::CopyFrom(const HeeksObj* object)
{
	operator=(*((CPocket*)object));
}

CPocket::CPocket() :CSketchOp(0, PocketType)
{
	m_step_over = 0.0;
	m_material_allowance = 0.0;
	m_starting_place = true;
	m_keep_tool_down_if_poss = true;
	m_use_zig_zag = true;
	m_zig_angle = 0.0;
	m_zig_unidirectional = false;
	m_entry_move = ePlunge;
	m_cut_mode = eConventional;
}

CPocket::CPocket( const CPocket & rhs ) : CSketchOp(rhs)
{
	m_step_over = rhs.m_step_over;
	m_material_allowance = rhs.m_material_allowance;
	m_starting_place = rhs.m_starting_place;
	m_keep_tool_down_if_poss = rhs.m_keep_tool_down_if_poss;
	m_use_zig_zag = rhs.m_use_zig_zag;
	m_zig_angle = rhs.m_zig_angle;
	m_zig_unidirectional = rhs.m_zig_unidirectional;
	m_entry_move = rhs.m_entry_move;
	m_cut_mode = rhs.m_cut_mode;
}

CPocket & CPocket::operator= ( const CPocket & rhs )
{
	if (this != &rhs)
	{
		CSketchOp::operator=(rhs);
		m_step_over = rhs.m_step_over;
		m_material_allowance = rhs.m_material_allowance;
		m_starting_place = rhs.m_starting_place;
		m_keep_tool_down_if_poss = rhs.m_keep_tool_down_if_poss;
		m_use_zig_zag = rhs.m_use_zig_zag;
		m_zig_angle = rhs.m_zig_angle;
		m_zig_unidirectional = rhs.m_zig_unidirectional;
		m_entry_move = rhs.m_entry_move;
		m_cut_mode = rhs.m_cut_mode;
	}

	return(*this);
}

bool CPocket::CanAddTo(HeeksObj* owner)
{
	return ((owner != NULL) && (owner->GetType() == OperationsType));
}

void CPocket::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = new TiXmlElement( "Pocket" );
	wxGetApp().LinkXMLEndChild( root,  element );
	WriteXMLAttributes(element);

	WriteBaseXML(element);
}

// static member function
HeeksObj* CPocket::ReadFromXMLElement(TiXmlElement* element)
{
	CPocket* new_object = new CPocket;

	std::list<TiXmlElement *> elements_to_remove;

	// read parameters
	TiXmlElement* params = wxGetApp().FirstNamedXMLChildElement(element, "params");
	if(params)
	{
		new_object->ReadParamsFromXMLElement(params);
		elements_to_remove.push_back(params);
	}

	for (std::list<TiXmlElement*>::iterator itElem = elements_to_remove.begin(); itElem != elements_to_remove.end(); itElem++)
	{
		wxGetApp().RemoveXMLChild( element, *itElem);
	}

	// read common parameters
	new_object->ReadBaseXML(element);

	return new_object;
}

CPocket::CPocket(int sketch, const int tool_number )
	: CSketchOp(sketch, tool_number, PocketType )
{
	ReadDefaultValues();
	set_initial_values(tool_number);
}


/**
	The old version of the CDrilling object stored references to graphics as type/id pairs
	that get read into the m_symbols list.  The new version stores these graphics references
	as child elements (based on ObjList).  If we read in an old-format file then the m_symbols
	list will have data in it for which we don't have children.  This routine converts
	these type/id pairs into the HeeksObj pointers as children.
 */

static void on_set_spline_deviation(double value, HeeksObj* object){
	CPocket::max_deviation_for_spline_to_arc = value;
	CPocket::WriteToConfig();
}

// static
void CPocket::GetOptions(std::list<Property *> *list)
{
#if 0 // to do
	list->push_back(new PropertyDouble(_("Pocket spline deviation"), max_deviation_for_spline_to_arc, NULL, on_set_spline_deviation));
#endif
}

// static
void CPocket::ReadFromConfig()
{
	HeeksConfig config;
	config.Read(_T("PocketSplineDeviation"), &max_deviation_for_spline_to_arc, 0.1);
}

// static
void CPocket::WriteToConfig()
{
	HeeksConfig config;
	config.Write(_T("PocketSplineDeviation"), max_deviation_for_spline_to_arc);
}

void CPocket::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
    CSketchOp::GetTools( t_list, p );
}

bool CPocket::operator==(const CPocket & rhs) const
{
	if (m_starting_place != rhs.m_starting_place) return(false);
	if (m_material_allowance != rhs.m_material_allowance) return(false);
	if (m_step_over != rhs.m_step_over) return(false);
	if (m_keep_tool_down_if_poss != rhs.m_keep_tool_down_if_poss) return(false);
	if (m_use_zig_zag != rhs.m_use_zig_zag) return(false);
	if (m_zig_angle != rhs.m_zig_angle) return(false);
	if (m_zig_unidirectional != rhs.m_zig_unidirectional) return(false);
	if (m_entry_move != rhs.m_entry_move) return(false);

	return(CSketchOp::operator==(rhs));
}

static bool OnEdit(HeeksObj* object)
{
	return PocketDlg::Do((CPocket*)object);
}

void CPocket::GetOnEdit(bool(**callback)(HeeksObj*))
{
	*callback = OnEdit;
}

bool CPocket::Add(HeeksObj* object, HeeksObj* prev_object)
{
	return CSketchOp::Add(object, prev_object);
}
