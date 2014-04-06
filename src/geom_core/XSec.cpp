//
// This file is released under the terms of the NASA Open Source Agreement (NOSA)
// version 1.3 as detailed in the LICENSE file which accompanies this software.
//

// XSec.cpp: implementation of the XSec class.
//
//////////////////////////////////////////////////////////////////////

#include "XSec.h"
#include "Geom.h"
#include "XSecSurf.h"
#include "Parm.h"
#include "VehicleMgr.h"
#include "ParmMgr.h"
#include "StlHelper.h"
#include <float.h>
#include <stdio.h>

#include "Vehicle.h"

using std::string;

//==== Default Constructor ====//
XSec::XSec( XSecCurve *xsc, bool use_left )
{
    m_XSCurve = xsc;

    if ( m_XSCurve  )
    {
        m_XSCurve->SetParentContainer( m_ID );
    }

    m_rotation.loadIdentity();
    m_center = false;

    m_Type = FUSE_SEC;

    m_GroupName = "XSec";
    m_RefLength = 1.0;
    m_XLocPercent.Init( "XLocPercent", m_GroupName, this,  0.0, 0.0, 1.0 );
    m_XLocPercent.SetDescript( "X distance of cross section as a percent of fuselage length" );
    m_YLocPercent.Init( "YLocPercent", m_GroupName, this,  0.0, -1.0, 1.0 );
    m_YLocPercent.SetDescript( "Y distance of cross section as a percent of fuselage length" );
    m_ZLocPercent.Init( "ZLocPercent", m_GroupName, this,  0.0, -1.0, 1.0 );
    m_ZLocPercent.SetDescript( "Z distance of cross section as a percent of fuselage length" );

    m_XRotate.Init( "XRotate", m_GroupName, this,  0.0, -180.0, 180.0 );
    m_XRotate.SetDescript( "Rotation about x-axis of cross section" );
    m_YRotate.Init( "YRotate", m_GroupName, this,  0.0, -180.0, 180.0 );
    m_YRotate.SetDescript( "Rotation about y-axis of cross section" );
    m_ZRotate.Init( "ZRotate", m_GroupName, this,  0.0, -180.0, 180.0 );
    m_YRotate.SetDescript( "Rotation about z-axis of cross section" );

    m_Spin.Init( "Spin", m_GroupName, this, 0.0, -180.0, 180.0 );

}


void XSec::ChangeID( string newid )
{
    string oldid = m_ID;
    ParmContainer::ChangeID( newid );

    XSecSurf* xssurf = ( XSecSurf* ) GetParentContainerPtr();

    xssurf->ChangeXSecID( oldid, newid );

    if ( m_XSCurve  )
    {
        m_XSCurve->SetParentContainer( newid );
    }
}

void XSec::SetGroupDisplaySuffix( int num )
{
    //==== Assign Group Suffix To All Parms ====//
    for ( int i = 0 ; i < ( int )m_ParmVec.size() ; i++ )
    {
        Parm* p = ParmMgr.FindParm( m_ParmVec[i] );
        if ( p )
        {
            p->SetGroupDisplaySuffix( num );
        }
    }

    if ( m_XSCurve )
    {
        m_XSCurve->SetGroupDisplaySuffix( num );
    }
}

//==== Set Ref Length ====//
void XSec::SetRefLength( double len )
{
    if ( fabs( len - m_RefLength ) < DBL_EPSILON )
    {
        return;
    }

    m_RefLength = len;
    m_LateUpdateFlag = true;

    m_XLocPercent.SetRefVal( m_RefLength );
    m_YLocPercent.SetRefVal( m_RefLength );
    m_ZLocPercent.SetRefVal( m_RefLength );
}

//==== Set Scale ====//
void XSec::SetScale( double scale )
{
    GetXSecCurve()->SetScale( scale );
}

//==== Parm Changed ====//
void XSec::ParmChanged( Parm* parm_ptr, int type )
{
    if ( type == Parm::SET )
    {
        m_LateUpdateFlag = true;
        return;
    }

    Update();

    //==== Notify Parent Container (XSecSurf) ====//
    ParmContainer* pc = GetParentContainerPtr();
    if ( pc )
    {
        pc->ParmChanged( parm_ptr, type );
    }
}

//==== Update ====//
void XSec::Update()
{
    m_LateUpdateFlag = false;

    // apply the needed transformation to get section into body orientation
    Matrix4d mat( m_rotation );
    double *pm( mat.data() );

    pm[3] = 0;
    pm[7] = 0;
    pm[11] = 0;
    pm[12] = 0;
    pm[13] = 0;
    pm[14] = 0;
    pm[15] = 0;
    if ( m_center )
    {
        pm[13] = -m_XSCurve->GetWidth() / 2;
    }

    VspCurve baseCurve = GetUntransformedCurve();

    baseCurve.Transform( mat );

    //==== Apply Transform ====//
    m_TransformedCurve = baseCurve;
    if ( fabs( m_Spin() ) > DBL_EPSILON )
    {
        std::cerr << "XSec spin not implemented." << std::endl;
// NOTE: Not implementing spin. Also, this implementation doesn't spin first or last segments
//      double val = 0.0;
//      if ( m_Spin() > 0 )         val = 0.5*(m_Spin()/180.0);
//      else if ( m_Spin() < 0 )    val = 1.0 + 0.5*(m_Spin()/180.0);
//      val = Clamp( val, 0.0, 0.99999999 );
//
//      vector< vec3d > ctl_pnts = m_Curve.GetControlPnts();
//      int split = (int)(val*(((int)ctl_pnts.size()-1)/3)+1);
//
//      vector< vec3d > spin_ctl_pnts;
//      for ( int i = split*3 ; i < (int)ctl_pnts.size()-1 ; i++ )
//          spin_ctl_pnts.push_back( ctl_pnts[i] );
//
//      for ( int i = 0 ; i <= split*3 ; i++ )
//          spin_ctl_pnts.push_back( ctl_pnts[i] );
//
//      m_TransformedCurve.SetControlPnts( spin_ctl_pnts );
    }

    m_TransformedCurve.RotateX( m_XRotate()*DEG_2_RAD );
    m_TransformedCurve.RotateY( m_YRotate()*DEG_2_RAD );
    m_TransformedCurve.RotateZ( m_ZRotate()*DEG_2_RAD );

    m_TransformedCurve.OffsetX( m_XLocPercent()*m_RefLength );
    m_TransformedCurve.OffsetY( m_YLocPercent()*m_RefLength );
    m_TransformedCurve.OffsetZ( m_ZLocPercent()*m_RefLength );

}

//==== Get Curve ====//
VspCurve& XSec::GetCurve()
{
    if ( m_LateUpdateFlag )
    {
        Update();
    }

    return m_TransformedCurve;
}

//==== Get Untransformed Curve ====//
VspCurve& XSec::GetUntransformedCurve()
{
    return m_XSCurve->GetCurve();
}

//==== Look Though All Parms and Load Linkable Ones ===//
void XSec::AddLinkableParms( vector< string > & parm_vec, const string & link_container_id )
{
    ParmContainer::AddLinkableParms( parm_vec, link_container_id );

    if ( m_XSCurve  )
    {
        m_XSCurve->AddLinkableParms( parm_vec, link_container_id );
    }
}

//==== Copy From XSec ====//
void XSec::CopyFrom( XSec* xs )
{
    ParmMgr.ResetRemapID();
    xmlNodePtr root = xmlNewNode( NULL, ( const xmlChar * )"Vsp_Geometry" );
    if ( xs->GetType() == GetType() && xs->GetXSecCurve()->GetType() == GetXSecCurve()->GetType() )
    {
        xs->EncodeXml( root );
        DecodeXml( root );
    }
    else
    {
        xs->XSec::EncodeXml( root );
        DecodeXml( root );

        m_XSCurve->SetWidthHeight( xs->GetXSecCurve()->GetWidth(), xs->GetXSecCurve()->GetHeight() );
    }
    xmlFreeNode( root );
    ParmMgr.ResetRemapID();
}

//==== Encode XML ====//
xmlNodePtr XSec::EncodeXml(  xmlNodePtr & node  )
{
    ParmContainer::EncodeXml( node );
    xmlNodePtr xsec_node = xmlNewChild( node, NULL, BAD_CAST "XSec", NULL );
    if ( xsec_node )
    {
        XmlUtil::AddIntNode( xsec_node, "Type", m_Type );
        XmlUtil::AddStringNode( xsec_node, "GroupName", m_GroupName );

        xmlNodePtr xscrv_node = xmlNewChild( xsec_node, NULL, BAD_CAST "XSecCurve", NULL );
        if ( xscrv_node )
        {
            m_XSCurve->EncodeXml( xscrv_node );
        }
    }
    return xsec_node;
}

//==== Decode XML ====//
// Called from XSec::DecodeXSec, XSec::CopyFrom, and overridden calls to ParmContainer::DecodeXml --
// i.e. during DecodeXml entire Geom, but also for in-XSecSurf copy/paste/insert.
xmlNodePtr XSec::DecodeXml(  xmlNodePtr & node  )
{
    ParmContainer::DecodeXml( node );

    xmlNodePtr child_node = XmlUtil::GetNode( node, "XSec", 0 );
    if ( child_node )
    {
        m_GroupName = XmlUtil::FindString( child_node, "GroupName", m_GroupName );

        xmlNodePtr xscrv_node = XmlUtil::GetNode( child_node, "XSecCurve", 0 );
        if ( xscrv_node )
        {
            m_XSCurve->DecodeXml( xscrv_node );
        }
    }
    return child_node;
}

//==== Encode XSec ====//
xmlNodePtr XSec::EncodeXSec(  xmlNodePtr & node  )
{
    xmlNodePtr xsec_node = xmlNewChild( node, NULL, BAD_CAST "XSec", NULL );
    if ( xsec_node )
    {
        EncodeXml( xsec_node );
    }
    return xsec_node;
}

//==== Decode XSec ====//
// Called only from XSecSurf::DecodeXml -- i.e. when DecodeXml'ing entire Geom.
xmlNodePtr XSec::DecodeXSec(  xmlNodePtr & node   )
{
    if ( node )
    {
        DecodeXml( node );
    }
    return node;
}

//==== Compute Area ====//
double XSec::ComputeArea( int num_pnts )
{
    VspCurve curve = GetCurve();
    vector<vec3d> pnts;
    curve.Tesselate( num_pnts, pnts );
    vec3d zero;
    return poly_area( pnts, zero );
}

void XSec::SetTransformation( const Matrix4d &mat, bool center )
{
    m_rotation = mat;
    m_center = center;
}

//==========================================================================//
//==========================================================================//
//==========================================================================//
