//
// This file is released under the terms of the NASA Open Source Agreement (NOSA)
// version 1.3 as detailed in the LICENSE file which accompanies this software.
//

// FeaStructure.cpp
//
// Justin Gravett
//////////////////////////////////////////////////////////////////////

#include "FeaStructure.h"

#include "Vehicle.h"
#include "VehicleMgr.h"
#include "ParmMgr.h"
#include "StructureMgr.h"
#include "LinkMgr.h"
#include "WingGeom.h"
#include "ConformalGeom.h"

#include <cfloat>

//////////////////////////////////////////////////////
//================== FeaStructure ==================//
//////////////////////////////////////////////////////

FeaStructure::FeaStructure( string geomID, int surf_index )
{
    m_ParentGeomID = geomID;
    m_MainSurfIndx = surf_index;

    m_FeaPartCount = 0;
    m_FeaSubSurfCount = 0;

    LinkMgr.RegisterContainer( m_StructSettings.GetID() );
    LinkMgr.RegisterContainer( m_FeaGridDensity.GetID() );
}

FeaStructure::~FeaStructure()
{
    // Delete FeaParts
    for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
    {
        delete m_FeaPartVec[i];
    }
    m_FeaPartVec.clear();

    // Delete SubSurfaces
    for ( int i = 0; i < (int)m_FeaSubSurfVec.size(); i++ )
    {
        delete m_FeaSubSurfVec[i];
    }
    m_FeaSubSurfVec.clear();
}

void FeaStructure::Update()
{
    UpdateFeaParts();
    UpdateFeaSubSurfs();
}

xmlNodePtr FeaStructure::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_info = xmlNewChild( node, NULL, BAD_CAST "FeaStructureInfo", NULL );

    XmlUtil::AddStringNode( fea_info, "ParentGeomID", m_ParentGeomID );
    XmlUtil::AddIntNode( fea_info, "MainSurfIndx", m_MainSurfIndx );

    for ( unsigned int i = 0; i < m_FeaPartVec.size(); i++ )
    {
        m_FeaPartVec[i]->EncodeXml( fea_info );
    }

    for ( unsigned int i = 0; i < m_FeaSubSurfVec.size(); i++ )
    {
        xmlNodePtr sub_node = xmlNewChild( fea_info, NULL, BAD_CAST "FeaSubSurface", NULL );

        if ( sub_node )
        {
            m_FeaSubSurfVec[i]->EncodeXml( sub_node );
        }
    }

    m_StructSettings.EncodeXml( fea_info );
    m_FeaGridDensity.EncodeXml( fea_info );

    return fea_info;
}

xmlNodePtr FeaStructure::DecodeXml( xmlNodePtr & node )
{
    int numparts = XmlUtil::GetNumNames( node, "FeaPartInfo" );

    for ( unsigned int i = 0; i < numparts; i++ )
    {
        xmlNodePtr part_info = XmlUtil::GetNode( node, "FeaPartInfo", i );

        if ( part_info )
        {
            int type = XmlUtil::FindInt( part_info, "FeaPartType", 0 );

            if ( type != vsp::FEA_SKIN )
            {
                FeaPart* feapart = AddFeaPart( type );
                feapart->DecodeXml( part_info );
            }
            else
            {
                FeaPart* feaskin = new FeaSkin( m_ParentGeomID );
                feaskin->DecodeXml( part_info );
                m_FeaPartVec.push_back( feaskin );
            }
        }
    }

    int num_ss = XmlUtil::GetNumNames( node, "FeaSubSurface" );

    for ( int ss = 0; ss < num_ss; ss++ )
    {
        xmlNodePtr ss_node = XmlUtil::GetNode( node, "FeaSubSurface", ss );
        if ( ss_node )
        {
            xmlNodePtr ss_info_node = XmlUtil::GetNode( ss_node, "SubSurfaceInfo", 0 );
            if ( ss_info_node )
            {
                int type = XmlUtil::FindInt( ss_info_node, "Type", vsp::SS_LINE );

                SubSurface* ssurf = AddFeaSubSurf( type );
                if ( ssurf )
                {
                    ssurf->DecodeXml( ss_node );
                }
            }
        }
    }

    return node;
}

void FeaStructure::SetDrawFlag( bool flag )
{
    for ( size_t i = 0; i < m_FeaPartVec.size(); i++ )
    {
        m_FeaPartVec[i]->m_DrawFeaPartFlag.Set( flag );
    }

    for ( size_t i = 0; i < m_FeaSubSurfVec.size(); i++ )
    {
        m_FeaSubSurfVec[i]->m_DrawFeaPartFlag.Set( flag );
    }
}

FeaPart* FeaStructure::AddFeaPart( int type )
{
    FeaPart* feaprt = new FeaPart( m_ParentGeomID, type );

    if ( type == vsp::FEA_SLICE )
    {
        feaprt = new FeaSlice( m_ParentGeomID );
        feaprt->SetName( string( "Slice_" + std::to_string( m_FeaPartCount ) ) );
    }
    else if ( type == vsp::FEA_RIB )
    {
        feaprt = new FeaRib( m_ParentGeomID );
        feaprt->SetName( string( "Rib_" + std::to_string( m_FeaPartCount ) ) );
    }
    else if ( type == vsp::FEA_SPAR )
    {
        feaprt = new FeaSpar( m_ParentGeomID );
        feaprt->SetName( string( "Spar_" + std::to_string( m_FeaPartCount ) ) );
    }
    else if ( type == vsp::FEA_FIX_POINT )
    {
        // Initially define the FeaFixPoint on the skin surface
        FeaPart* skin = GetFeaSkin();

        if ( skin )
        {
            feaprt = new FeaFixPoint( m_ParentGeomID, skin->GetID() );
            feaprt->SetName( string( "FixPoint_" + std::to_string( m_FeaPartCount ) ) );
        }
    }
    else if ( type == vsp::FEA_DOME )
    {
        feaprt = new FeaDome( m_ParentGeomID );
        feaprt->SetName( string( "Dome_" + std::to_string( m_FeaPartCount ) ) );
    }
    else if ( type == vsp::FEA_RIB_ARRAY )
    {
        feaprt = new FeaRibArray( m_ParentGeomID );
        feaprt->SetName( string( "RibArray_" + std::to_string( m_FeaPartCount ) ) );
    }
    else if ( type == vsp::FEA_SLICE_ARRAY )
    {
        feaprt = new FeaSliceArray( m_ParentGeomID );
        feaprt->SetName( string( "SliceArray_" + std::to_string( m_FeaPartCount ) ) );
    }

    if ( feaprt )
    {
        feaprt->m_MainSurfIndx.Set( m_MainSurfIndx );
        AddFeaPart( feaprt );
    }

    m_FeaPartCount++;

    return feaprt;
}

void FeaStructure::DelFeaPart( int ind )
{
    if ( ValidFeaPartInd( ind ) )
    {
        delete m_FeaPartVec[ind];
        m_FeaPartVec.erase( m_FeaPartVec.begin() + ind );
    }
}

void FeaStructure::ReorderFeaPart( int ind, int action )
{
    //==== Check SubSurface Index Validity ====//
    if ( !ValidFeaPartInd( ind ) )
    {
        return;
    }

    vector < FeaPart* > new_prt_vec;

    if ( action == Vehicle::REORDER_MOVE_TOP || action == Vehicle::REORDER_MOVE_BOTTOM )
    {
        if ( action == Vehicle::REORDER_MOVE_TOP )
        {
            new_prt_vec.push_back( GetFeaPart( ind ) );
        }

        for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
            if ( m_FeaPartVec[i] != GetFeaPart( ind ) )
            {
                new_prt_vec.push_back( m_FeaPartVec[i] );
            }

        if ( action == Vehicle::REORDER_MOVE_BOTTOM )
        {
            new_prt_vec.push_back( GetFeaPart( ind ) );
        }
    }
    else if ( action == Vehicle::REORDER_MOVE_UP || action == Vehicle::REORDER_MOVE_DOWN )
    {
        for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
        {
            if ( i < (int)( m_FeaPartVec.size() - 1 ) &&
                ( ( action == Vehicle::REORDER_MOVE_DOWN && m_FeaPartVec[i] == GetFeaPart( ind ) ) ||
                 ( action == Vehicle::REORDER_MOVE_UP   && m_FeaPartVec[i + 1] == GetFeaPart( ind ) ) ) )
            {
                new_prt_vec.push_back( m_FeaPartVec[i + 1] );
                new_prt_vec.push_back( m_FeaPartVec[i] );
                i++;
            }
            else
            {
                new_prt_vec.push_back( m_FeaPartVec[i] );
            }
        }
    }

    m_FeaPartVec = new_prt_vec;
}

//==== Highlight Active Subsurface ====//
void FeaStructure::RecolorFeaSubSurfs( vector < int > active_ind_vec )
{
    for ( int i = 0; i < (int)m_FeaSubSurfVec.size(); i++ )
    {
        m_FeaSubSurfVec[i]->SetLineColor( vec3d( 0, 0, 0 ) ); // Initially color all black
    }

    for ( size_t j = 0; j < active_ind_vec.size(); j++ )
    {
        for ( int i = 0; i < (int)m_FeaSubSurfVec.size(); i++ )
        {
            if ( i == active_ind_vec[j] )
            {
                m_FeaSubSurfVec[i]->SetLineColor( vec3d( 1, 0, 0 ) );
            }
        }
    }
}

SubSurface* FeaStructure::AddFeaSubSurf( int type )
{
    SubSurface* ssurf = NULL;

    if ( type == vsp::SS_LINE )
    {
        ssurf = new SSLine( m_ParentGeomID );
        ssurf->SetName( string( "SSLINE_" + to_string( m_FeaSubSurfCount ) ) );
    }
    else if ( type == vsp::SS_RECTANGLE )
    {
        ssurf = new SSRectangle( m_ParentGeomID );
        ssurf->SetName( string( "SSRect_" + to_string( m_FeaSubSurfCount ) ) );
    }
    else if ( type == vsp::SS_ELLIPSE )
    {
        ssurf = new SSEllipse( m_ParentGeomID );
        ssurf->SetName( string( "SSEllipse_" + to_string( m_FeaSubSurfCount ) ) );
    }
    else if ( type == vsp::SS_CONTROL )
    {
        ssurf = new SSControlSurf( m_ParentGeomID );
        ssurf->SetName( string( "SSConSurf_" + to_string( m_FeaSubSurfCount ) ) );
    }
    else if ( type == vsp::SS_LINE_ARRAY )
    {
        ssurf = new SSLineArray( m_ParentGeomID );
        ssurf->SetName( string( "SSLineArray_" + to_string( m_FeaSubSurfCount ) ) );
    }

    if ( ssurf )
    {
        ssurf->m_MainSurfIndx.Set( m_MainSurfIndx );
        m_FeaSubSurfVec.push_back( ssurf );
    }

    m_FeaSubSurfCount++;

    return ssurf;
}

bool FeaStructure::ValidFeaSubSurfInd( int ind )
{
    if ( (int)m_FeaSubSurfVec.size() > 0 && ind >= 0 && ind < (int)m_FeaSubSurfVec.size() )
    {
        return true;
    }
    else
    {
        return false;
    }
}

void FeaStructure::DelFeaSubSurf( int ind )
{
    if ( ValidFeaSubSurfInd( ind ) )
    {
        delete m_FeaSubSurfVec[ind];
        m_FeaSubSurfVec.erase( m_FeaSubSurfVec.begin() + ind );
    }
}

SubSurface* FeaStructure::GetFeaSubSurf( int ind )
{
    if ( ValidFeaSubSurfInd( ind ) )
    {
        return m_FeaSubSurfVec[ind];
    }
    return NULL;
}

void FeaStructure::ReorderFeaSubSurf( int ind, int action )
{
    //==== Check SubSurface Index Validity ====//
    if ( !ValidFeaSubSurfInd( ind ) )
    {
        return;
    }

    vector < SubSurface* > new_ss_vec;

    if ( action == Vehicle::REORDER_MOVE_TOP || action == Vehicle::REORDER_MOVE_BOTTOM )
    {
        if ( action == Vehicle::REORDER_MOVE_TOP )
        {
            new_ss_vec.push_back( GetFeaSubSurf( ind ) );
        }

        for ( int i = 0; i < (int)m_FeaSubSurfVec.size(); i++ )
            if ( m_FeaSubSurfVec[i] != GetFeaSubSurf( ind ) )
            {
                new_ss_vec.push_back( m_FeaSubSurfVec[i] );
            }

        if ( action == Vehicle::REORDER_MOVE_BOTTOM )
        {
            new_ss_vec.push_back( GetFeaSubSurf( ind ) );
        }
    }
    else if ( action == Vehicle::REORDER_MOVE_UP || action == Vehicle::REORDER_MOVE_DOWN )
    {
        for ( int i = 0; i < (int)m_FeaSubSurfVec.size(); i++ )
        {
            if ( i < (int)( m_FeaSubSurfVec.size() - 1 ) &&
                ( ( action == Vehicle::REORDER_MOVE_DOWN && m_FeaSubSurfVec[i] == GetFeaSubSurf( ind ) ) ||
                 ( action == Vehicle::REORDER_MOVE_UP   && m_FeaSubSurfVec[i + 1] == GetFeaSubSurf( ind ) ) ) )
            {
                new_ss_vec.push_back( m_FeaSubSurfVec[i + 1] );
                new_ss_vec.push_back( m_FeaSubSurfVec[i] );
                i++;
            }
            else
            {
                new_ss_vec.push_back( m_FeaSubSurfVec[i] );
            }
        }
    }

    m_FeaSubSurfVec = new_ss_vec;
}

bool FeaStructure::ValidFeaPartInd( int ind )
{
    if ( (int)m_FeaPartVec.size() > 0 && ind >= 0 && ind < (int)m_FeaPartVec.size() )
    {
        return true;
    }
    else
    {
        return false;
    }
}

void FeaStructure::UpdateFeaParts()
{
    for ( unsigned int i = 0; i < m_FeaPartVec.size(); i++ )
    {
        m_FeaPartVec[i]->UpdateSymmIndex();

        if ( FeaPartIsFixPoint( i ) )
        {
            FeaFixPoint* fixpt = dynamic_cast<FeaFixPoint*>( m_FeaPartVec[i] );
            assert( fixpt );

            // Store HalfMeshFlag setting
            fixpt->m_HalfMeshFlag = m_StructSettings.GetHalfMeshFlag();
        }

        m_FeaPartVec[i]->Update();

        if ( !FeaPartIsFixPoint( i ) && !FeaPartIsArray( i ) )
        {
            // Symmetric FixedPoints and Arrays are updated in their respective Update functions
            m_FeaPartVec[i]->UpdateSymmParts();
        }
    }
}

void FeaStructure::UpdateFeaSubSurfs()
{
    for ( unsigned int i = 0; i < m_FeaSubSurfVec.size(); i++ )
    {
        m_FeaSubSurfVec[i]->Update();
    }
}

vector < FeaPart* > FeaStructure::InitFeaSkin()
{
    m_FeaPartVec.clear();

    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( veh )
    {
        Geom* currgeom = veh->FindGeom( m_ParentGeomID );

        if ( currgeom )
        {
            FeaPart* feaskin;
            feaskin = new FeaSkin( m_ParentGeomID );

            if ( feaskin )
            {
                feaskin->SetName( string( "Skin" ) );
                feaskin->m_MainSurfIndx.Set( m_MainSurfIndx );
                
                feaskin->UpdateSymmIndex();
                feaskin->Update();
                feaskin->UpdateSymmParts();

                m_FeaPartVec.push_back( feaskin );
            }
        }
    }

    return m_FeaPartVec;
}

FeaPart* FeaStructure::GetFeaPart( int ind )
{
    if ( ValidFeaPartInd( ind ) )
    {
        return m_FeaPartVec[ind];
    }
    return NULL;
}

string FeaStructure::GetFeaPartName( int ind )
{
    string name;
    FeaPart* fea_part = GetFeaPart( ind );

    if ( fea_part )
    {
        name = fea_part->GetName();
    }
    return name;
}

bool FeaStructure::FeaPartIsFixPoint( int ind )
{
    bool fixpoint = false;
    FeaPart* fea_part = GetFeaPart( ind );

    if ( fea_part )
    {
        if ( fea_part->GetType() == vsp::FEA_FIX_POINT )
        {
            fixpoint = true;
        }
    }
    return fixpoint;
}

int FeaStructure::GetNumFeaFixPoints()
{
    int fix_point_count = 0;

    for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
    {
        if ( FeaPartIsFixPoint( i ) )
        {
            fix_point_count++;
        }
    }
    return fix_point_count;
}

bool FeaStructure::FeaPartIsArray( int ind )
{
    bool array = false;
    FeaPart* fea_part = GetFeaPart( ind );

    if ( fea_part )
    {
        if ( fea_part->GetType() == vsp::FEA_RIB_ARRAY || fea_part->GetType() == vsp::FEA_SLICE_ARRAY )
        {
            array = true;
        }
    }
    return array;
}

void FeaStructure::IndividualizeRibArray( int rib_array_ind )
{
    if ( !ValidFeaPartInd( rib_array_ind ) )
    {
        return;
    }

    FeaPart* prt = m_FeaPartVec[rib_array_ind];

    if ( !prt )
    {
        return;
    }

    if ( prt->GetType() == vsp::FEA_RIB_ARRAY )
    {
        FeaRibArray* rib_array = dynamic_cast<FeaRibArray*>( prt );
        assert( rib_array );

        for ( size_t i = 0; i < rib_array->GetNumRibs(); i++ )
        {
            double center_location;

            if ( rib_array->m_AbsRelParmFlag() == vsp::REL )
            {
                center_location = rib_array->m_RelStartLocation() + i * rib_array->m_RibRelSpacing();
            }
            else if ( rib_array->m_AbsRelParmFlag() == vsp::ABS )
            {
                center_location = rib_array->m_AbsStartLocation() + i * rib_array->m_RibAbsSpacing();
            }

            FeaRib* rib = rib_array->AddFeaRib( center_location, i );
            AddFeaPart( rib );
        }

        DelFeaPart( rib_array_ind );
    }
}

void FeaStructure::IndividualizeSliceArray( int stiffener_array_ind )
{
    if ( !ValidFeaPartInd( stiffener_array_ind ) )
    {
        return;
    }

    FeaPart* prt = m_FeaPartVec[stiffener_array_ind];

    if ( !prt )
    {
        return;
    }

    if ( prt->GetType() == vsp::FEA_SLICE_ARRAY )
    {
        FeaSliceArray* slice_array = dynamic_cast<FeaSliceArray*>( prt );
        assert( slice_array );

        for ( size_t i = 0; i < slice_array->GetNumSlices(); i++ )
        {
            double center_location;

            if ( slice_array->m_AbsRelParmFlag() == vsp::REL )
            {
                center_location = slice_array->m_RelStartLocation() + i * slice_array->m_SliceRelSpacing();
            }
            else if ( slice_array->m_AbsRelParmFlag() == vsp::ABS )
            {
                center_location = slice_array->m_AbsStartLocation() + i * slice_array->m_SliceAbsSpacing();
            }

            FeaSlice* slice = slice_array->AddFeaSlice( center_location, i );
            AddFeaPart( slice );
        }

        DelFeaPart( stiffener_array_ind );
    }
}

void FeaStructure::IndividualizeSSLineArray( int ssline_array_ind )
{
    if ( !ValidFeaSubSurfInd( ssline_array_ind ) )
    {
        return;
    }

    SubSurface* sub_surf = m_FeaSubSurfVec[ssline_array_ind];

    if ( !sub_surf )
    {
        return;
    }

    if ( sub_surf->GetType() == vsp::SS_LINE_ARRAY )
    {
        SSLineArray* ssline_array = dynamic_cast<SSLineArray*>( sub_surf );
        assert( ssline_array );

        for ( size_t i = 0; i < ssline_array->GetNumLines(); i++ )
        {
            double center_location = ssline_array->m_StartLocation() + i * ssline_array->m_Spacing();

            SSLine* ssline = ssline_array->AddSSLine( center_location, i );
            AddFeaSubSurf( ssline );
        }

        DelFeaSubSurf( ssline_array_ind );
    }
}

FeaPart* FeaStructure::GetFeaSkin()
{
    for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
    {
        if ( m_FeaPartVec[i]->GetType() == vsp::FEA_SKIN )
        {
            return m_FeaPartVec[i];
        }
    }
    return NULL;
}

int FeaStructure::GetNumFeaSkin()
{
    int num_skin = 0;

    for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
    {
        if ( m_FeaPartVec[i]->GetType() == vsp::FEA_SKIN )
        {
            num_skin++;
        }
    }
    return num_skin;
}

//==== Get FeaProperty Index from FeaPart Index =====//
int FeaStructure::GetFeaPropertyIndex( int fea_part_ind )
{
    if ( ValidFeaPartInd( fea_part_ind ) )
    {
        FeaPart* fea_part = GetFeaPart( fea_part_ind );
        if ( fea_part )
        {
            return fea_part->m_FeaPropertyIndex();
        }
    }
    return -1; // indicates an error
}

//==== Get Cap FeaProperty Index from FeaPart Index =====//
int FeaStructure::GetCapFeaPropertyIndex( int fea_part_ind )
{
    if ( ValidFeaPartInd( fea_part_ind ) )
    {
        FeaPart* fea_part = GetFeaPart( fea_part_ind );
        if ( fea_part )
        {
            return fea_part->m_CapFeaPropertyIndex();
        }
    }
    return -1; // indicates an error
}

int FeaStructure::GetFeaPartIndex( FeaPart* fea_prt )
{
    for ( int i = 0; i < (int)m_FeaPartVec.size(); i++ )
    {
        if ( m_FeaPartVec[i] == fea_prt )
        {
            return i;
        }
    }
    return -1; // indicates an error
}

void FeaStructure::BuildSuppressList( vector < double > &usuppress, vector < double > &wsuppress )
{

    FeaSkin* skin = NULL;
    FeaPart* pskin = GetFeaSkin();
    if ( pskin )
    {
        skin = dynamic_cast< FeaSkin* > ( pskin );

        if ( !skin )
        {
            return;
        }
    }

    VspSurf* surf = skin->GetMainSurf();
    if ( surf )
    {
        vector < double > ufeature = surf->GetUFeature();
        vector < double > wfeature = surf->GetWFeature();

        double umax = surf->GetUMax();
        double wmax = surf->GetWMax();

        for ( int i = 0; i < ufeature.size(); i++ )
        {
            int npts = 5;
            vector < vec3d > pnts( npts );

            for ( int j = 0; j < npts; j++ )
            {
                double w = wmax * j * 1.0 / ( npts - 1 );
                pnts[j] = surf->CompPnt( ufeature[i], w );
            }

            if ( PtsOnAnyPlanarPart( pnts ) )
            {
                usuppress.push_back( ufeature[i] );
            }
        }

        for ( int i = 0; i < wfeature.size(); i++ )
        {
            int npts = 5;
            vector < vec3d > pnts( npts );

            for ( int j = 0; j < npts; j++ )
            {
                double u = umax * j * 1.0 / ( npts - 1 );
                pnts[j] = surf->CompPnt( u, wfeature[i] );
            }

            if ( PtsOnAnyPlanarPart( pnts ) )
            {
                wsuppress.push_back( wfeature[i] );
            }
        }
    }
}

bool FeaStructure::PtsOnAnyPlanarPart( const vector < vec3d > &pnts )
{
    // Loop over all parts.
    for ( int i  = 0; i < m_FeaPartVec.size(); i++ )
    {
        FeaPart* p = m_FeaPartVec[i];
        if ( p )
        {
            if ( p->PtsOnPlanarPart( pnts ) )
            {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////
//==================== FeaPart =====================//
//////////////////////////////////////////////////////

FeaPart::FeaPart( string geomID, int type )
{
    m_FeaPartType = type;
    m_ParentGeomID = geomID;

    m_MainSurfIndx.Init( "MainSurfIndx", "FeaPart", this, -1, -1, 1e12 );
    m_MainSurfIndx.SetDescript( "Surface Index for FeaPart" );

    m_IncludedElements.Init( "IncludedElements", "FeaPart", this, vsp::FEA_SHELL, vsp::FEA_SHELL, vsp::FEA_SHELL_AND_BEAM );
    m_IncludedElements.SetDescript( "Indicates the FeaElements to be Included for the FeaPart" );

    m_DrawFeaPartFlag.Init( "DrawFeaPartFlag", "FeaPart", this, true, false, true );
    m_DrawFeaPartFlag.SetDescript( "Flag to Draw FeaPart" );

    m_AbsRelParmFlag.Init( "AbsRelParmFlag", "FeaPart", this, vsp::REL, vsp::ABS, vsp::REL );
    m_AbsRelParmFlag.SetDescript( "Parameterization of Center Location as Absolute or Relative" );

    m_AbsCenterLocation.Init( "AbsCenterLocation", "FeaPart", this, 0.0, 0.0, 1e12 );
    m_AbsCenterLocation.SetDescript( "The Absolute Location of the Center of the FeaPart" );

    m_RelCenterLocation.Init( "RelCenterLocation", "FeaPart", this, 0.5, 0.0, 1.0 );
    m_RelCenterLocation.SetDescript( "The Relative Location of the Center of the FeaPart" );

    m_FeaPropertyIndex.Init( "FeaPropertyIndex", "FeaPart", this, 0, 0, 1e12 );; // Shell property default
    m_FeaPropertyIndex.SetDescript( "FeaPropertyIndex for Shell Elements" );

    m_CapFeaPropertyIndex.Init( "CapFeaPropertyIndex", "FeaPart", this, 1, 0, 1e12 );; // Beam property default
    m_CapFeaPropertyIndex.SetDescript( "FeaPropertyIndex for Beam (Cap) Elements" );
}

FeaPart::~FeaPart()
{

}

void FeaPart::Update()
{

}

void FeaPart::ParmChanged( Parm* parm_ptr, int type )
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        veh->ParmChanged( parm_ptr, type );
    }
}

xmlNodePtr FeaPart::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr part_info = xmlNewChild( node, NULL, BAD_CAST "FeaPartInfo", NULL );

    XmlUtil::AddIntNode( part_info, "FeaPartType", m_FeaPartType );

    return ParmContainer::EncodeXml( part_info );
}

xmlNodePtr FeaPart::DecodeXml( xmlNodePtr & node )
{
    return ParmContainer::DecodeXml( node );
}

void FeaPart::UpdateSymmParts()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_geom = veh->FindGeom( m_ParentGeomID );
        if ( !current_geom || m_FeaPartSurfVec.size() == 0 )
        {
            return;
        }

        vector< VspSurf > surf_vec;
        current_geom->GetSurfVec( surf_vec );

        // Get Symmetric Translation Matrix
        vector<Matrix4d> transMats = current_geom->GetFeaTransMatVec();

        //==== Apply Transformations ====//
        for ( int i = 1; i < m_SymmIndexVec.size(); i++ )
        {
            m_FeaPartSurfVec[i].Transform( transMats[i] ); // Apply total transformation to main FeaPart surface

            if ( surf_vec[i].GetFlipNormal() != m_FeaPartSurfVec[i].GetFlipNormal() )
            {
                m_FeaPartSurfVec[i].FlipNormal();
            }
        }
    }
}

void FeaPart::UpdateSymmIndex()
{
    m_SymmIndexVec.clear();
    m_FeaPartSurfVec.clear();

    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( !veh )
    {
        return;
    }

    Geom* currgeom = veh->FindGeom( m_ParentGeomID );

    if ( currgeom )
    {
        vector< VspSurf > surf_vec;
        currgeom->GetSurfVec( surf_vec );

        m_SymmIndexVec = currgeom->GetSymmIndexs( m_MainSurfIndx() );

        int ncopy = currgeom->GetNumSymmCopies();

        assert( ncopy == m_SymmIndexVec.size() );

        m_FeaPartSurfVec.resize( m_SymmIndexVec.size() );
    }
}

string FeaPart::GetTypeName( int type )
{
    if ( type == vsp::FEA_SLICE )
    {
        return string( "Slice" );
    }
    if ( type == vsp::FEA_RIB )
    {
        return string( "Rib" );
    }
    if ( type == vsp::FEA_SPAR )
    {
        return string( "Spar" );
    }
    if ( type == vsp::FEA_FIX_POINT )
    {
        return string( "FixPoint" );
    }
    if ( type == vsp::FEA_SKIN )
    {
        return string( "Skin" );
    }
    if ( type == vsp::FEA_RIB_ARRAY )
    {
        return string( "RibArray" );
    }
    if ( type == vsp::FEA_DOME )
    {
        return string( "Dome" );
    }
    if ( type == vsp::FEA_SLICE_ARRAY )
    {
        return string( "SliceArray" );
    }

    return string( "NONE" );
}

double FeaPart::GetRibPerU( double rel_center_location )
{
    double per_u = 0;
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( current_wing )
        {

            WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
            assert( wing );

            vector< VspSurf > surf_vec;
            current_wing->GetSurfVec( surf_vec );
            VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

            BndBox wing_bbox;
            wing_surf.GetBoundingBox( wing_bbox );

            int num_wing_sec = wing->NumXSec();

            vector < double > wing_sec_span_vec; // Vector of Span lengths for each wing section (first section has no length)
            wing_sec_span_vec.push_back( 0.0 );

            double U_max = wing_surf.GetUMax();

            // Init values:
            double span_0 = 0.0;
            double span_f = 0.0;
            double U_0, U_f;
            int curr_sec_ind = -1;

            // Get total span
            double span = 0.0;

            for ( size_t i = 1; i < num_wing_sec; i++ )
            {
                WingSect* wing_sec = wing->GetWingSect( i );

                if ( wing_sec )
                {
                    span += wing_sec->m_Span();
                }
            }

            // Determine current wing section:
            for ( size_t i = 1; i < num_wing_sec; i++ )
            {
                WingSect* wing_sec = wing->GetWingSect( i );

                if ( wing_sec )
                {
                    span_f += wing_sec->m_Span();
                    wing_sec_span_vec.push_back( span_f - span_0 );

                    if ( rel_center_location >= span_0 && rel_center_location <= span_f )
                    {
                        curr_sec_ind = i;
                    }

                    span_0 = span_f;
                }
            }

            if ( wing->m_CapUMinOption() == vsp::NO_END_CAP )
            {
                U_0 = ( curr_sec_ind - 1 );
            }
            else
            {
                U_0 = curr_sec_ind;
            }

            U_f = U_0 + 1;

            double u_step = ( U_f - U_0 ) / U_max;

            per_u = U_0 / U_max + ( ( ( rel_center_location * span ) - wing_sec_span_vec[curr_sec_ind - 1] ) / wing_sec_span_vec[curr_sec_ind] ) * u_step;
        }
    }

    return per_u;
}

double FeaPart::GetRibTotalRotation( double rel_center_location, double initial_rotation, string perp_edge_ID )
{
    double total_rot = 0;

    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( current_wing )
        {
            vector< VspSurf > surf_vec;
            current_wing->GetSurfVec( surf_vec );
            VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

            double per_u = GetRibPerU( rel_center_location );

            // Find initial rotation (alpha) to perpendicular edge or spar
            double alpha = 0.0;
            double u_edge_out = per_u + 2 * FLT_EPSILON;
            double u_edge_in = per_u - 2 * FLT_EPSILON;

            VspCurve constant_u_curve;
            wing_surf.GetU01ConstCurve( constant_u_curve, per_u );

            piecewise_curve_type u_curve = constant_u_curve.GetCurve();

            double v_min = u_curve.get_parameter_min(); // Really must be 0.0
            double v_max = u_curve.get_parameter_max(); // Really should be 4.0
            double v_leading_edge = ( v_min + v_max ) * 0.5;

            vec3d trail_edge, lead_edge;
            trail_edge = u_curve.f( v_min );
            lead_edge = u_curve.f( v_leading_edge );

            vec3d chord_dir_vec = trail_edge - lead_edge;
            chord_dir_vec.normalize();

            // Wing corner points:
            vec3d min_trail_edge = wing_surf.CompPnt( 0.0, 0.0 );
            vec3d min_lead_edge = wing_surf.CompPnt( 0.0, v_leading_edge );

            // Wing edge vectors (assumes linearity)
            vec3d lead_edge_vec = lead_edge - min_lead_edge;
            vec3d inner_edge_vec = min_trail_edge - min_lead_edge;

            lead_edge_vec.normalize();
            inner_edge_vec.normalize();

            // Normal vector to wing chord line
            vec3d normal_vec = cross( inner_edge_vec, lead_edge_vec );
            normal_vec.normalize();

            if ( strcmp( perp_edge_ID.c_str(), "Trailing Edge" ) == 0 )
            {
                vec3d trail_edge_out, trail_edge_in;
                trail_edge_out = wing_surf.CompPnt01( u_edge_out, v_min );
                trail_edge_in = wing_surf.CompPnt01( u_edge_in, v_min );

                vec3d trail_edge_dir_vec = trail_edge_out - trail_edge_in;
                trail_edge_dir_vec.normalize();

                alpha = ( PI / 2 ) - signed_angle( chord_dir_vec, trail_edge_dir_vec, normal_vec );
            }
            else if ( strcmp( perp_edge_ID.c_str(), "Leading Edge" ) == 0 )
            {
                vec3d lead_edge_out, lead_edge_in;
                lead_edge_out = wing_surf.CompPnt01( u_edge_out, v_leading_edge / v_max );
                lead_edge_in = wing_surf.CompPnt01( u_edge_in, v_leading_edge / v_max );

                vec3d lead_edge_dir_vec = lead_edge_out - lead_edge_in;
                lead_edge_dir_vec.normalize();

                alpha = ( PI / 2 ) - signed_angle( chord_dir_vec, lead_edge_dir_vec, normal_vec );
            }
            else if ( strcmp( perp_edge_ID.c_str(), "None" ) == 0 )
            {
                alpha = 0;
            }
            else 
            {
                FeaPart* part = StructureMgr.GetFeaPart( perp_edge_ID );

                if ( part )
                {
                    VspSurf surf = part->GetFeaPartSurfVec()[0];

                    vec3d edge1, edge2;
                    edge1 = surf.CompPnt01( 0.5, 0.0 );
                    edge2 = surf.CompPnt01( 0.5, 1.0 );

                    vec3d spar_dir_vec = edge2 - edge1;
                    spar_dir_vec.normalize();

                    alpha = ( PI / 2 ) - signed_angle( chord_dir_vec, spar_dir_vec, normal_vec );
                }
            }

            total_rot = alpha + initial_rotation;
        }
    }

    return total_rot;
}

VspSurf FeaPart::ComputeRibSurf( double rel_center_location, double rotation )
{
    VspSurf rib_surf;
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing )
        {
            return rib_surf;
        }
        rib_surf = VspSurf(); // Create primary VspSurf

        if ( m_IncludedElements() == vsp::FEA_SHELL || m_IncludedElements() == vsp::FEA_SHELL_AND_BEAM )
        {
            rib_surf.SetSurfCfdType( vsp::CFD_STRUCTURE );
        }
        else
        {
            rib_surf.SetSurfCfdType( vsp::CFD_STIFFENER );
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        vector< VspSurf > surf_vec;
        current_wing->GetSurfVec( surf_vec );
        VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

        BndBox wing_bbox;
        wing_surf.GetBoundingBox( wing_bbox );

        // Get center location as percent of U
        double per_u = GetRibPerU( rel_center_location );

        VspCurve constant_u_curve;
        wing_surf.GetU01ConstCurve( constant_u_curve, per_u );

        piecewise_curve_type u_curve = constant_u_curve.GetCurve();

        double v_min = u_curve.get_parameter_min(); // Really must be 0.0
        double v_max = u_curve.get_parameter_max(); // Really should be 4.0
        double v_leading_edge = ( v_min + v_max ) * 0.5;

        vec3d trail_edge, lead_edge;
        trail_edge = u_curve.f( v_min );
        lead_edge = u_curve.f( v_leading_edge );

        // Find two points slightly above and below the trailing edge
        double v_trail_edge_low = v_min + 2 * TMAGIC;
        double v_trail_edge_up = v_max - 2 * TMAGIC;

        vec3d trail_edge_up, trail_edge_low;
        trail_edge_up = u_curve.f( v_trail_edge_low );
        trail_edge_low = u_curve.f( v_trail_edge_up );

        vec3d wing_z_axis = trail_edge_up - trail_edge_low;
        wing_z_axis.normalize();

        vec3d center = ( trail_edge + lead_edge ) / 2; // Center of rib

        // Wing corner points:
        vec3d min_trail_edge = wing_surf.CompPnt( 0.0, 0.0 );
        vec3d min_lead_edge = wing_surf.CompPnt( 0.0, v_leading_edge );
        vec3d max_trail_edge = wing_surf.CompPnt( wing_surf.GetUMax(), 0.0 );
        vec3d max_lead_edge = wing_surf.CompPnt( wing_surf.GetUMax(), v_leading_edge );

        // Wing edge vectors (assumes linearity)
        vec3d trail_edge_vec = max_trail_edge - min_trail_edge;
        vec3d lead_edge_vec = max_lead_edge - min_lead_edge;
        vec3d inner_edge_vec = min_lead_edge - min_trail_edge;
        vec3d outer_edge_vec = max_lead_edge - min_trail_edge;

        trail_edge_vec.normalize();
        lead_edge_vec.normalize();
        inner_edge_vec.normalize();
        outer_edge_vec.normalize();

        vec3d center_to_trail_edge = trail_edge - center;
        center_to_trail_edge.normalize();

        vec3d center_to_lead_edge = center - lead_edge;
        center_to_lead_edge.normalize();

        // Identify expansion 
        double expan = wing_bbox.GetLargestDist() * 1e-5;
        if ( expan < 1e-6 )
        {
            expan = 1e-6;
        }

        double length_rib_0 = ( dist( trail_edge, lead_edge ) / 2 ) + expan; // Rib half length before rotations, slightly oversized

        // Normal vector to wing chord line
        vec3d normal_vec;
        
        if ( inner_edge_vec.mag() >= FLT_EPSILON )
        {
            normal_vec = cross( lead_edge_vec, inner_edge_vec );
        }
        else
        {
            normal_vec = cross( lead_edge_vec, outer_edge_vec );
        }

        normal_vec.normalize();

        // Determine angle between center and corner points
        vec3d center_to_le_min_vec = min_lead_edge - center;
        vec3d center_to_te_min_vec = min_trail_edge - center;
        vec3d center_to_le_max_vec = max_lead_edge - center;
        vec3d center_to_te_max_vec = max_trail_edge - center;

        center_to_le_min_vec.normalize();
        center_to_te_min_vec.normalize();
        center_to_le_max_vec.normalize();
        center_to_te_max_vec.normalize();

        // Get maximum angles for rib to intersect wing edges
        double max_angle_inner_le = -PI + signed_angle( center_to_le_min_vec, center_to_lead_edge, normal_vec );
        double max_angle_inner_te = signed_angle( center_to_te_min_vec, center_to_trail_edge, normal_vec );
        double max_angle_outer_le = PI - signed_angle( center_to_lead_edge, center_to_le_max_vec, normal_vec );
        double max_angle_outer_te = signed_angle( center_to_te_max_vec, center_to_trail_edge, normal_vec );

        double sweep_te = -1* signed_angle( trail_edge_vec, center_to_trail_edge, normal_vec ); // Trailing edge sweep
        double sweep_le = -1 * signed_angle( lead_edge_vec, center_to_lead_edge, normal_vec ); // Leading edge sweep

        double phi_te = PI - ( rotation + sweep_te ); // Total angle for trailing edge side of rib
        double phi_le = PI - ( rotation + sweep_le );// Total angle for leading edge side of rib

        double length_rib_te, length_rib_le, perp_dist;

        // Determine if the rib intersects the leading/trailing edge or inner/outer edge
        if ( rotation <= 0 )
        {
            if ( rotation <= max_angle_inner_le )
            {
                if ( abs( sin( rotation ) ) <= FLT_EPSILON || ( min_lead_edge - min_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_rib_le = length_rib_0;
                }
                else
                {
                    perp_dist = cross( ( center - min_trail_edge ), ( center - min_lead_edge ) ).mag() / ( min_lead_edge - min_trail_edge ).mag();
                    length_rib_le = abs( perp_dist / sin( rotation ) );
                }
            }
            else
            {
                if ( abs( sin( phi_le ) ) <= FLT_EPSILON )
                {
                    length_rib_le = length_rib_0;
                }
                else
                {
                    length_rib_le = abs( length_rib_0 * sin( sweep_le ) / sin( phi_le ) );
                }
            }

            if ( rotation <= max_angle_outer_te )
            {
                if ( abs( sin( rotation ) ) <= FLT_EPSILON || ( max_lead_edge - max_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_rib_te = length_rib_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_trail_edge ), ( center - max_lead_edge ) ).mag() / ( max_lead_edge - max_trail_edge ).mag();
                    length_rib_te = abs( perp_dist / sin( rotation ) );
                }
            }
            else
            {
                if ( abs( sin( phi_te ) ) <= FLT_EPSILON )
                {
                    length_rib_te = length_rib_0;
                }
                else
                {
                    length_rib_te = abs( length_rib_0 * sin( sweep_te ) / sin( phi_te ) );
                }
            }
        }
        else
        {
            if ( rotation >= max_angle_inner_te )
            {
                if ( abs( sin( rotation ) ) <= FLT_EPSILON || ( min_lead_edge - min_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_rib_te = length_rib_0;
                }
                else
                {
                    perp_dist = cross( ( center - min_trail_edge ), ( center - min_lead_edge ) ).mag() / ( min_lead_edge - min_trail_edge ).mag();
                    length_rib_te = abs( perp_dist / sin( rotation ) );
                }
            }
            else
            {
                if ( abs( sin( phi_te ) ) <= FLT_EPSILON )
                {
                    length_rib_te = length_rib_0;
                }
                else
                {
                    length_rib_te = abs( length_rib_0 * sin( sweep_te ) / sin( phi_te ) );
                }
            }

            if ( rotation >= max_angle_outer_le )
            {
                if ( abs( sin( rotation ) ) <= FLT_EPSILON || ( max_lead_edge - max_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_rib_le = length_rib_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_trail_edge ), ( center - max_lead_edge ) ).mag() / ( max_lead_edge - max_trail_edge ).mag();
                    length_rib_le = abs( perp_dist / sin( rotation ) );
                }
            }
            else
            {
                if ( abs( sin( phi_le ) ) <= FLT_EPSILON )
                {
                    length_rib_le = length_rib_0;
                }
                else
                {
                    length_rib_le = abs( length_rib_0 * sin( sweep_le ) / sin( phi_le ) );
                }
            }
        }

        // Apply Rodrigues' Rotation Formula
        vec3d rib_vec_te = center_to_trail_edge * cos( rotation ) + cross( center_to_trail_edge, normal_vec ) * sin( rotation ) + normal_vec * dot( center_to_trail_edge, normal_vec ) * ( 1 - cos( rotation ) );
        vec3d rib_vec_le = center_to_lead_edge * cos( rotation ) + cross( center_to_lead_edge, normal_vec ) * sin( rotation ) + normal_vec * dot( center_to_lead_edge, normal_vec ) * ( 1 - cos( rotation ) );

        rib_vec_te.normalize();
        rib_vec_le.normalize();

        // Calculate final end points
        vec3d trail_edge_f = center + length_rib_te * rib_vec_te;
        vec3d lead_edge_f = center - length_rib_le * rib_vec_le;

        // Identify corners of the plane
        vec3d cornerA, cornerB, cornerC, cornerD;

        double height = 0.5 * wing_bbox.GetSmallestDist() + expan; // Height of Rib, slightly oversized

        cornerA = trail_edge_f + ( height * wing_z_axis );
        cornerB = trail_edge_f - ( height * wing_z_axis );
        cornerC = lead_edge_f + ( height * wing_z_axis );
        cornerD = lead_edge_f - ( height * wing_z_axis );

        // Make Planar Surface
        rib_surf.MakePlaneSurf( cornerA, cornerB, cornerC, cornerD );

        if ( rib_surf.GetFlipNormal() != wing_surf.GetFlipNormal() )
        {
            rib_surf.FlipNormal();
        }
    }

    return rib_surf;
}

bool FeaPart::RefFrameIsBody( int orientation_plane )
{
    if ( orientation_plane == vsp::XY_BODY || orientation_plane == vsp::YZ_BODY || orientation_plane == vsp::XZ_BODY )
    {
        return true;
    }
    else
    {
        return false;
    }
}

VspSurf FeaPart::ComputeSliceSurf( double rel_center_location, int orientation_plane, double x_rot, double y_rot, double z_rot )
{
    VspSurf slice_surf;
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_geom = veh->FindGeom( m_ParentGeomID );
        if ( !current_geom )
        {
            return slice_surf;
        }

        vector< VspSurf > surf_vec;
        current_geom->GetSurfVec( surf_vec );
        VspSurf current_surf = surf_vec[m_MainSurfIndx()];

        slice_surf = VspSurf(); // Create primary VspSurf

        if ( m_IncludedElements() == vsp::FEA_SHELL || m_IncludedElements() == vsp::FEA_SHELL_AND_BEAM )
        {
            slice_surf.SetSurfCfdType( vsp::CFD_STRUCTURE );
        }
        else
        {
            slice_surf.SetSurfCfdType( vsp::CFD_STIFFENER );
        }

        // Determine BndBox dimensions prior to rotating and translating
        Matrix4d model_matrix = current_geom->getModelMatrix();
        model_matrix.affineInverse();

        VspSurf orig_surf = current_surf;
        orig_surf.Transform( model_matrix );

        double u_max = current_surf.GetUMax();

        vec3d slice_center, geom_center, cornerA, cornerB, cornerC, cornerD, x_axis, y_axis, z_axis,
            center_to_A, center_to_B, center_to_C, center_to_D;
        double del_x_plus, del_x_minus, del_y_plus, del_y_minus, del_z_plus, del_z_minus, max_length, del_x, del_y, del_z;

        x_axis.set_x( 1.0 );
        y_axis.set_y( 1.0 );
        z_axis.set_z( 1.0 );

        BndBox geom_bbox;

        if ( RefFrameIsBody( orientation_plane ) )
        {
            orig_surf.GetBoundingBox( geom_bbox );
        }
        else
        {
            current_surf.GetBoundingBox( geom_bbox );
        }
        geom_bbox.Expand( 0.5 );

        geom_center = geom_bbox.GetCenter();
        del_x = geom_bbox.GetMax( 0 ) - geom_bbox.GetMin( 0 );
        del_y = geom_bbox.GetMax( 1 ) - geom_bbox.GetMin( 1 );
        del_z = geom_bbox.GetMax( 2 ) - geom_bbox.GetMin( 2 );

        // Identify expansion 
        double expan = geom_bbox.GetLargestDist() * 1e-5;
        if ( expan < 1e-6 )
        {
            expan = 1e-6;
        }

        if ( orientation_plane == vsp::CONST_U )
        {
            // Build conformal spine from parent geom
            ConformalSpine cs;
            cs.Build( current_surf );

            double spine_length = cs.GetSpineLength();

            double length_on_spine = rel_center_location * spine_length;
            double per_u = cs.FindUGivenLengthAlongSpine( length_on_spine ) / u_max;

            slice_center = cs.FindCenterGivenU( per_u * u_max );

            // Use small change in u along spline to get x axis of geom at center point
            double delta_u;

            if ( per_u < ( 1.0 - 2 * FLT_EPSILON ) )
            {
                delta_u = ( per_u * u_max ) + ( 2 * FLT_EPSILON );
            }
            else
            {
                delta_u = ( per_u * u_max ) - ( 2 * FLT_EPSILON );
            }

            vec3d delta_u_center = cs.FindCenterGivenU( delta_u );

            x_axis = delta_u_center - slice_center;
            x_axis.normalize();

            vec3d surf_pnt1 = current_surf.CompPnt01( per_u, 0.0 );
            vec3d surf_pnt2 = current_surf.CompPnt01( per_u, 0.5 );

            z_axis = surf_pnt1 - surf_pnt2;
            z_axis.normalize();

            y_axis = cross( x_axis, z_axis );
            y_axis.normalize();

            VspCurve u_curve;
            current_surf.GetU01ConstCurve( u_curve, per_u );

            BndBox xsec_box;
            u_curve.GetBoundingBox( xsec_box );
            max_length = xsec_box.GetLargestDist() + 2 * FLT_EPSILON;

            // TODO: Improve initial size and resize after rotations

            // TODO: Improve 45 deg assumption
            vec3d y_prime = max_length * y_axis * cos( PI / 4 ) + max_length * z_axis * sin( PI / 4 );
            vec3d z_prime = max_length * -1 * y_axis * sin( PI / 4 ) + max_length * z_axis * cos( PI / 4 );

            cornerA = slice_center + y_prime;
            cornerB = slice_center - z_prime;
            cornerC = slice_center + z_prime;
            cornerD = slice_center - y_prime;
        }
        else
        {
            // Increase size slighlty to avoid tangency errors in FeaMeshMgr
            del_x_minus = expan;
            del_x_plus = expan;
            del_y_minus = expan;
            del_y_plus = expan;
            del_z_minus = expan;
            del_z_plus = expan;

            if ( orientation_plane == vsp::YZ_BODY || orientation_plane == vsp::YZ_ABS )
            {
                slice_center = vec3d( geom_bbox.GetMin( 0 ) + del_x * rel_center_location, geom_center.y(), geom_center.z() );

                double x_off = ( slice_center - geom_center ).x();

                // Resize for Y rotation
                if ( abs( DEG_2_RAD * y_rot ) > atan( ( del_x + 2 * x_off ) / del_z ) )
                {
                    del_z_plus += abs( ( del_x + 2 * x_off ) / sin( DEG_2_RAD * y_rot ) );
                }
                else
                {
                    del_z_plus += abs( del_z / cos( DEG_2_RAD * y_rot ) );
                }

                if ( abs( DEG_2_RAD * y_rot ) > atan( ( del_x - 2 * x_off ) / del_z ) )
                {
                    del_z_minus += abs( ( del_x - 2 * x_off ) / sin( DEG_2_RAD * y_rot ) );
                }
                else
                {
                    del_z_minus += abs( del_z / cos( DEG_2_RAD * y_rot ) );
                }

                // Resize for Z rotation
                if ( abs( DEG_2_RAD * z_rot ) > atan( ( del_x + 2 * x_off ) / del_y ) )
                {
                    del_y_minus += abs( ( del_x + 2 * x_off ) / sin( DEG_2_RAD * z_rot ) );
                }
                else
                {
                    del_y_minus += abs( del_y / cos( DEG_2_RAD * z_rot ) );
                }

                if ( abs( DEG_2_RAD * z_rot ) > atan( ( del_x - 2 * x_off ) / del_y ) )
                {
                    del_y_plus += abs( ( del_x - 2 * x_off ) / sin( DEG_2_RAD * z_rot ) );
                }
                else
                {
                    del_y_plus += abs( del_y / cos( DEG_2_RAD * z_rot ) );
                }

                // swap _plus and _minus if negative rotation
                if ( y_rot < 0.0 )
                {
                    double temp = del_z_plus;
                    del_z_plus = del_z_minus;
                    del_z_minus = temp;
                }

                if ( z_rot < 0.0 )
                {
                    double temp = del_y_plus;
                    del_y_plus = del_y_minus;
                    del_y_minus = temp;
                }

                center_to_A.set_y( -0.5 * del_y_minus );
                center_to_A.set_z( -0.5 * del_z_minus );

                center_to_B.set_y( 0.5 * del_y_plus );
                center_to_B.set_z( -0.5 * del_z_minus );

                center_to_C.set_y( -0.5 * del_y_minus );
                center_to_C.set_z( 0.5 * del_z_plus );

                center_to_D.set_y( 0.5 * del_y_plus );
                center_to_D.set_z( 0.5 * del_z_plus );
            }
            else if ( orientation_plane == vsp::XY_BODY || orientation_plane == vsp::XY_ABS )
            {

                slice_center = vec3d( geom_center.x(), geom_center.y(), geom_bbox.GetMin( 2 ) + del_z * rel_center_location );

                double z_off = ( slice_center - geom_center ).z();

                // Resize for Y rotation
                if ( abs( DEG_2_RAD * y_rot ) > atan( ( del_z + 2 * z_off ) / del_x ) )
                {
                    del_x_minus += abs( ( del_z + 2 * z_off ) / sin( DEG_2_RAD * y_rot ) );
                }
                else
                {
                    del_x_minus += abs( del_x / cos( DEG_2_RAD * y_rot ) );
                }

                if ( abs( DEG_2_RAD * y_rot ) > atan( ( del_z - 2 * z_off ) / del_x ) )
                {
                    del_x_plus += abs( ( del_z - 2 * z_off ) / sin( DEG_2_RAD * y_rot ) );
                }
                else
                {
                    del_x_plus += abs( del_x / cos( DEG_2_RAD * y_rot ) );
                }

                double test1 = atan( ( del_z + 2 * z_off ) / del_y );

                // Resize for X rotation
                if ( abs( DEG_2_RAD * x_rot ) > atan( ( del_z + 2 * z_off ) / del_y ) )
                {
                    del_y_plus += abs( ( del_z + 2 * z_off ) / sin( DEG_2_RAD * x_rot ) );
                }
                else
                {
                    del_y_plus += abs( del_y / cos( DEG_2_RAD * x_rot ) );
                }

                if ( abs( DEG_2_RAD * x_rot ) > atan( ( del_z - 2 * z_off ) / del_y ) )
                {
                    del_y_minus += abs( ( del_z - 2 * z_off ) / sin( DEG_2_RAD * x_rot ) );
                }
                else
                {
                    del_y_minus += abs( del_y / cos( DEG_2_RAD * x_rot ) );
                }

                // swap _plus and _minus if negative rotation
                if ( y_rot < 0.0 )
                {
                    double temp = del_x_plus;
                    del_x_plus = del_x_minus;
                    del_x_minus = temp;
                }

                if ( x_rot < 0.0 )
                {
                    double temp = del_y_plus;
                    del_y_plus = del_y_minus;
                    del_y_minus = temp;
                }

                center_to_A.set_x( -0.5 * del_x_minus );
                center_to_A.set_y( -0.5 * del_y_minus );

                center_to_B.set_x( -0.5 * del_x_minus );
                center_to_B.set_y( 0.5 * del_y_plus );

                center_to_C.set_x( 0.5 * del_x_plus );
                center_to_C.set_y( -0.5 * del_y_minus );

                center_to_D.set_x( 0.5 * del_x_plus );
                center_to_D.set_y( 0.5 * del_y_plus );
            }
            else if ( orientation_plane == vsp::XZ_BODY || orientation_plane == vsp::XZ_ABS )
            {
                slice_center = vec3d( geom_center.x(), geom_bbox.GetMin( 1 ) + del_y * rel_center_location, geom_center.z() );

                double y_off = ( slice_center - geom_center ).y();

                // Resize for Z rotation
                if ( abs( DEG_2_RAD * z_rot ) > atan( ( del_y + 2 * y_off ) / del_x ) )
                {
                    del_x_plus += abs( ( del_y + 2 * y_off ) / sin( DEG_2_RAD * z_rot ) );
                }
                else
                {
                    del_x_plus += abs( del_x / cos( DEG_2_RAD * z_rot ) );
                }

                if ( abs( DEG_2_RAD * z_rot ) > atan( ( del_y - 2 * y_off ) / del_x ) )
                {
                    del_x_minus += abs( ( del_y - 2 * y_off ) / sin( DEG_2_RAD * z_rot ) );
                }
                else
                {
                    del_x_minus += abs( del_x / cos( DEG_2_RAD * z_rot ) );
                }

                // Resize for X rotation
                if ( abs( DEG_2_RAD * x_rot ) > atan( ( del_y + 2 * y_off ) / del_z ) )
                {
                    del_z_minus += abs( ( del_y + 2 * y_off ) / sin( DEG_2_RAD * x_rot ) );
                }
                else
                {
                    del_z_minus += abs( del_z / cos( DEG_2_RAD * x_rot ) );
                }

                if ( abs( DEG_2_RAD * x_rot ) > atan( ( del_y - 2 * y_off ) / del_z ) )
                {
                    del_z_plus += abs( ( del_y - 2 * y_off ) / sin( DEG_2_RAD * x_rot ) );
                }
                else
                {
                    del_z_plus += abs( del_z / cos( DEG_2_RAD * x_rot ) );
                }

                // swap _plus and _minus if negative rotation
                if ( z_rot < 0.0 )
                {
                    double temp = del_x_plus;
                    del_x_plus = del_x_minus;
                    del_x_minus = temp;
                }

                if ( x_rot < 0.0 )
                {
                    double temp = del_z_plus;
                    del_z_plus = del_z_minus;
                    del_z_minus = temp;
                }

                center_to_A.set_x( -0.5 * del_x_minus );
                center_to_A.set_z( -0.5 * del_z_minus );

                center_to_B.set_x( 0.5 * del_x_plus );
                center_to_B.set_z( -0.5 * del_z_minus );

                center_to_C.set_x( -0.5 * del_x_minus );
                center_to_C.set_z( 0.5 * del_z_plus );

                center_to_D.set_x( 0.5 * del_x_plus );
                center_to_D.set_z( 0.5 * del_z_plus );
            }

            cornerA = slice_center + center_to_A;
            cornerB = slice_center + center_to_B;
            cornerC = slice_center + center_to_C;
            cornerD = slice_center + center_to_D;
        }

        // Make Planar Surface
        slice_surf.MakePlaneSurf( cornerA, cornerB, cornerC, cornerD );

        // Translate to the origin, rotate, and translate back to m_CenterPerBBoxLocation
        Matrix4d trans_mat_1, trans_mat_2, rot_mat_x, rot_mat_y, rot_mat_z;

        trans_mat_1.loadIdentity();
        trans_mat_1.translatef( slice_center.x() * -1, slice_center.y() * -1, slice_center.z() * -1 );
        slice_surf.Transform( trans_mat_1 );

        rot_mat_x.loadIdentity();
        rot_mat_x.rotate( DEG_2_RAD * x_rot, x_axis );
        slice_surf.Transform( rot_mat_x );

        rot_mat_y.loadIdentity();
        rot_mat_y.rotate( DEG_2_RAD * y_rot, y_axis );
        slice_surf.Transform( rot_mat_y );

        rot_mat_z.loadIdentity();
        rot_mat_z.rotate( DEG_2_RAD * z_rot, z_axis );
        slice_surf.Transform( rot_mat_z );

        trans_mat_2.loadIdentity();
        trans_mat_2.translatef( slice_center.x(), slice_center.y(), slice_center.z() );
        slice_surf.Transform( trans_mat_2 );

        if ( RefFrameIsBody( orientation_plane ) )
        {
            // Transform to body coordinate frame
            model_matrix.affineInverse();
            slice_surf.Transform( model_matrix );
        }
    }

    return slice_surf;
}

void FeaPart::FetchFeaXFerSurf( vector< XferSurf > &xfersurfs, int compid, const vector < double > &usuppress, const vector < double > &wsuppress )
{
    for ( int p = 0; p < m_FeaPartSurfVec.size(); p++ )
    {
        // CFD_STRUCTURE and CFD_STIFFENER type surfaces have m_CompID starting at -9999
        m_FeaPartSurfVec[p].FetchXFerSurf( m_ParentGeomID, m_MainSurfIndx(), compid, xfersurfs, usuppress, wsuppress );
    }
}

void FeaPart::LoadDrawObjs( std::vector< DrawObj* > & draw_obj_vec )
{
    for ( int i = 0; i < (int)m_FeaPartDO.size(); i++ )
    {
        draw_obj_vec.push_back( &m_FeaPartDO[i] );
    }
}

void FeaPart::UpdateDrawObjs( int id, bool highlight )
{
    m_FeaPartDO.clear();
    m_FeaPartDO.resize( m_FeaPartSurfVec.size() );

    for ( unsigned int j = 0; j < m_FeaPartSurfVec.size(); j++ )
    {
        m_FeaPartDO[j].m_PntVec.clear();

        m_FeaPartDO[j].m_GeomID = string( m_Name + "_" + std::to_string( id ) + "_" + std::to_string( j ) );
        m_FeaPartDO[j].m_Screen = DrawObj::VSP_MAIN_SCREEN;

        if ( highlight )
        {
            m_FeaPartDO[j].m_LineColor = vec3d( 1.0, 0.0, 0.0 );
            m_FeaPartDO[j].m_LineWidth = 3.0;
        }
        else
        {
            m_FeaPartDO[j].m_LineColor = vec3d( 96.0 / 255.0, 96.0 / 255.0, 96.0 / 255.0 );
            m_FeaPartDO[j].m_LineWidth = 1.0;
        }

        m_FeaPartDO[j].m_Type = DrawObj::VSP_WIRE_SHADED_QUADS;

        vec3d p00 = m_FeaPartSurfVec[j].CompPnt01( 0, 0 );
        vec3d p10 = m_FeaPartSurfVec[j].CompPnt01( 1, 0 );
        vec3d p11 = m_FeaPartSurfVec[j].CompPnt01( 1, 1 );
        vec3d p01 = m_FeaPartSurfVec[j].CompPnt01( 0, 1 );

        m_FeaPartDO[j].m_PntVec.push_back( p00 );
        m_FeaPartDO[j].m_PntVec.push_back( p10 );
        m_FeaPartDO[j].m_PntVec.push_back( p11 );
        m_FeaPartDO[j].m_PntVec.push_back( p01 );

        // Get new normal
        vec3d quadnorm = cross( p10 - p00, p01 - p00 );
        quadnorm.normalize();

        for ( int i = 0; i < 4; i++ )
        {
            m_FeaPartDO[j].m_NormVec.push_back(quadnorm);
        }

        // Set plane color to medium glass
        for ( int i = 0; i < 4; i++ )
        {
            m_FeaPartDO[j].m_MaterialInfo.Ambient[i] = 0.2f;
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[i] = 0.1f;
            m_FeaPartDO[j].m_MaterialInfo.Specular[i] = 0.7f;
            m_FeaPartDO[j].m_MaterialInfo.Emission[i] = 0.0f;
        }

        if ( highlight )
        {
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[3] = 0.67f;
        }
        else
        {
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[3] = 0.33f;
        }

        m_FeaPartDO[j].m_MaterialInfo.Shininess = 5.0f;


        m_FeaPartDO[j].m_GeomChanged = true;
    }
}

int FeaPart::GetFeaMaterialIndex()
{
    FeaProperty* fea_prop = StructureMgr.GetFeaProperty( m_FeaPropertyIndex() );

    if ( fea_prop )
    {
        return fea_prop->m_FeaMaterialIndex();
    }
    else
    {
        return -1;
    }
}

void FeaPart::SetFeaMaterialIndex( int index )
{
    FeaProperty* fea_prop = StructureMgr.GetFeaProperty( m_FeaPropertyIndex() );

    if ( fea_prop )
    {
        fea_prop->m_FeaMaterialIndex.Set( index );
    }
}

VspSurf* FeaPart::GetMainSurf()
{
    VspSurf* retsurf = NULL;

    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( veh )
    {
        Geom *currgeom = veh->FindGeom( m_ParentGeomID );
        if ( currgeom )
        {
            retsurf = currgeom->GetSurfPtr( m_MainSurfIndx() );
        }
    }
    return retsurf;
}

bool FeaPart::PtsOnPlanarPart( const vector < vec3d > & pnts )
{
    double tol = 1.0e-6;

    VspSurf surf = m_FeaPartSurfVec[0];

    double umax = surf.GetUMax();
    double wmax = surf.GetWMax();

    vec3d o = surf.CompPnt( umax * 0.5, wmax * 0.5 );
    vec3d n = surf.CompNorm( umax * 0.5, wmax * 0.5 );

    // Find point furthest from surface.
    double dmax = 0.0;
    for ( int i = 0; i < pnts.size(); i++ )
    {
        double d;

        vec3d p = pnts[i];
        d = dist_pnt_2_plane( o, n, p );

        if ( d > dmax )
        {
            dmax = d;
        }
    }

    // If furthest point is within tolerance, all points are on surface.
    if ( dmax < tol )
    {
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////
//==================== FeaSlice ====================//
//////////////////////////////////////////////////////

FeaSlice::FeaSlice( string geomID, int type ) : FeaPart( geomID, type )
{
    m_OrientationPlane.Init( "OrientationPlane", "FeaSlice", this, vsp::YZ_BODY, vsp::XY_BODY, vsp::CONST_U );
    m_OrientationPlane.SetDescript( "Plane the FeaSlice Part will be Parallel to (Body or Absolute Reference Frame)" );

    m_RotationAxis.Init( "RotationAxis", "FeaSlice", this, vsp::X_DIR, vsp::X_DIR, vsp::Z_DIR );
    m_XRot.Init( "XRot", "FeaSlice", this, 0.0, -90.0, 90.0 );
    m_YRot.Init( "YRot", "FeaSlice", this, 0.0, -90.0, 90.0 );
    m_ZRot.Init( "ZRot", "FeaSlice", this, 0.0, -90.0, 90.0 );
}

void FeaSlice::Update()
{
    UpdateParmLimits();

    // Must call UpdateSymmIndex before
    if ( m_FeaPartSurfVec.size() > 0 )
    {
        m_FeaPartSurfVec[0] = ComputeSliceSurf( m_RelCenterLocation(), m_OrientationPlane(), m_XRot(), m_YRot(), m_ZRot() );

        // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copies to be definied in UpdateSymmParts 
        for ( unsigned int j = 1; j < m_SymmIndexVec.size(); j++ )
        {
            m_FeaPartSurfVec[j] = m_FeaPartSurfVec[j - 1];
        }
    }
    // Must call UpdateSymmParts next
}

void FeaSlice::UpdateParmLimits()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_geom = veh->FindGeom( m_ParentGeomID );
        if ( !current_geom )
        {
            return;
        }

        vector< VspSurf > surf_vec;
        current_geom->GetSurfVec( surf_vec );
        VspSurf current_surf = surf_vec[m_MainSurfIndx()];

        // Determine BndBox dimensions prior to rotating and translating
        Matrix4d model_matrix = current_geom->getModelMatrix();
        model_matrix.affineInverse();

        VspSurf orig_surf = current_surf;
        orig_surf.Transform( model_matrix );

        BndBox geom_bbox;

        if ( RefFrameIsBody( m_OrientationPlane() ) )
        {
            orig_surf.GetBoundingBox( geom_bbox );
        }
        else
        {
            current_surf.GetBoundingBox( geom_bbox );
        }

        double perp_dist; // Total distance perpendicular to the FeaSlice plane

        if ( m_OrientationPlane() == vsp::XY_BODY || m_OrientationPlane() == vsp::XY_ABS )
        {
            perp_dist = geom_bbox.GetMax( 2 ) - geom_bbox.GetMin( 2 );
        }
        else if ( m_OrientationPlane() == vsp::YZ_BODY || m_OrientationPlane() == vsp::YZ_ABS )
        {
            perp_dist = geom_bbox.GetMax( 0 ) - geom_bbox.GetMin( 0 );
        }
        else if ( m_OrientationPlane() == vsp::XZ_BODY || m_OrientationPlane() == vsp::XZ_ABS )
        {
            perp_dist = geom_bbox.GetMax( 1 ) - geom_bbox.GetMin( 1 );
        }
        else if ( m_OrientationPlane() == vsp::CONST_U )
        {
            // Build conformal spine from parent geom
            ConformalSpine cs;
            cs.Build( current_surf );

            perp_dist = cs.GetSpineLength();
        }

        // Set Parm limits and values
        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            m_AbsCenterLocation.Set( m_RelCenterLocation() * perp_dist );
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            m_AbsCenterLocation.SetUpperLimit( perp_dist );
            m_RelCenterLocation.Set( m_AbsCenterLocation() / perp_dist );
        }
    }
}

void FeaSlice::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart::UpdateDrawObjs( id, highlight );
}

//////////////////////////////////////////////////////
//===================== FeaSpar ====================//
//////////////////////////////////////////////////////

FeaSpar::FeaSpar( string geomID, int type ) : FeaSlice( geomID, type )
{
    m_Theta.Init( "Theta", "FeaSpar", this, 0.0, -90.0, 90.0 );

    m_LimitSparToSectionFlag.Init( "LimitSparToSectionFlag", "FeaSpar", this, false, false, true );
    m_LimitSparToSectionFlag.SetDescript( "Flag to Limit Spar Length to Wing Section" );

    m_CurrWingSection.Init( "CurrWingSection", "FeaSpar", this, 1, 1, 1000 );
    m_CurrWingSection.SetDescript( "Current Wing Section to Limit Spar Length to" );
}

void FeaSpar::Update()
{
    UpdateParms();
    ComputePlanarSurf();
}

void FeaSpar::UpdateParms()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing || m_FeaPartSurfVec.size() == 0 )
        {
            return;
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        vector< VspSurf > surf_vec;
        current_wing->GetSurfVec( surf_vec );
        VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

        int num_wing_sec = wing->NumXSec();
        int U_max = wing_surf.GetUMax();

        m_CurrWingSection.SetUpperLimit( num_wing_sec - 1 );

        double U_sec_min, U_sec_max;

        // Determine U limits of spar
        if ( m_LimitSparToSectionFlag() )
        {
            if ( wing->m_CapUMinOption() == vsp::NO_END_CAP )
            {
                U_sec_min = ( m_CurrWingSection() - 1 );
            }
            else
            {
                U_sec_min = m_CurrWingSection();
            }

            U_sec_max = U_sec_min + 1;
        }
        else
        {
            if ( wing->m_CapUMinOption() == vsp::NO_END_CAP )
            {
                U_sec_min = 0;
            }
            else
            {
                U_sec_min = 1;
            }
            if ( wing->m_CapUMaxOption() == vsp::NO_END_CAP )
            {
                U_sec_max = U_max;
            }
            else
            {
                U_sec_max = U_max - 1;
            }
        }

        double u_mid = ( ( U_sec_min + U_sec_max ) / 2 ) / U_max;

        double chord_length = dist( wing_surf.CompPnt01( u_mid, 0.5 ), wing_surf.CompPnt01( u_mid, 0.0 ) ); // average chord length

        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            m_AbsCenterLocation.Set( m_RelCenterLocation() * chord_length );
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            m_AbsCenterLocation.SetUpperLimit( chord_length );
            m_RelCenterLocation.Set( m_AbsCenterLocation() / chord_length );
        }
    }
}

void FeaSpar::ComputePlanarSurf()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing || m_FeaPartSurfVec.size() == 0 )
        {
            return;
        }

        m_FeaPartSurfVec[0] = VspSurf(); // Create primary VspSurf

        if ( m_IncludedElements() == vsp::FEA_SHELL || m_IncludedElements() == vsp::FEA_SHELL_AND_BEAM )
        {
            m_FeaPartSurfVec[0].SetSurfCfdType( vsp::CFD_STRUCTURE );
        }
        else
        {
            m_FeaPartSurfVec[0].SetSurfCfdType( vsp::CFD_STIFFENER );
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        vector< VspSurf > surf_vec;
        current_wing->GetSurfVec( surf_vec );
        VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

        BndBox wing_bbox;
        wing_surf.GetBoundingBox( wing_bbox );

        int num_wing_sec = wing->NumXSec();
        int U_max = wing_surf.GetUMax();

        m_CurrWingSection.SetUpperLimit( num_wing_sec - 1 );

        double U_sec_min, U_sec_max;

        // Determine U limits of spar
        if ( m_LimitSparToSectionFlag() )
        {
            if ( wing->m_CapUMinOption() == vsp::NO_END_CAP )
            {
                U_sec_min = ( m_CurrWingSection() - 1 );
            }
            else
            {
                U_sec_min = m_CurrWingSection();
            }

            U_sec_max = U_sec_min + 1;
        }
        else
        {
            if ( wing->m_CapUMinOption() == vsp::NO_END_CAP )
            {
                U_sec_min = 0;
            }
            else
            {
                U_sec_min = 1;
            }
            if ( wing->m_CapUMaxOption() == vsp::NO_END_CAP )
            {
                U_sec_max = U_max;
            }
            else
            {
                U_sec_max = U_max - 1;
            }
        }

        double u_mid = ( ( U_sec_min + U_sec_max ) / 2 ) / U_max;

        VspCurve constant_u_curve;
        wing_surf.GetU01ConstCurve( constant_u_curve, u_mid );

        piecewise_curve_type u_curve = constant_u_curve.GetCurve();

        double V_min = u_curve.get_parameter_min(); // Really must be 0.0
        double V_max = u_curve.get_parameter_max(); // Really should be 4.0
        double V_leading_edge = ( V_min + V_max ) * 0.5;

        // Wing corner points:
        vec3d min_trail_edge = wing_surf.CompPnt( U_sec_min, 0.0 );
        vec3d min_lead_edge = wing_surf.CompPnt( U_sec_min, V_leading_edge );
        vec3d max_trail_edge = wing_surf.CompPnt( U_sec_max, 0.0 );
        vec3d max_lead_edge = wing_surf.CompPnt( U_sec_max, V_leading_edge );

        // Determine inner edge and outer edge spar points before rotations
        vec3d inside_edge_vec = min_lead_edge - min_trail_edge;
        double inside_edge_length = inside_edge_vec.mag();
        inside_edge_vec.normalize();
        vec3d inside_edge_pnt = min_lead_edge - ( m_RelCenterLocation() * inside_edge_length ) * inside_edge_vec;

        vec3d outside_edge_vec = max_lead_edge - max_trail_edge;
        double outside_edge_length = outside_edge_vec.mag();
        outside_edge_vec.normalize();
        vec3d outside_edge_pnt = max_lead_edge - ( m_RelCenterLocation() * outside_edge_length ) * outside_edge_vec;

        double length_spar_0 = dist( inside_edge_pnt, outside_edge_pnt ) / 2; // Initial spar half length

        // Find two points slightly above and below the trailing edge
        double v_trail_edge_low = V_min + 2 * TMAGIC;
        double v_trail_edge_up = V_max - 2 * TMAGIC;

        vec3d trail_edge_up, trail_edge_low;
        trail_edge_up = u_curve.f( v_trail_edge_low );
        trail_edge_low = u_curve.f( v_trail_edge_up );

        vec3d wing_z_axis = trail_edge_up - trail_edge_low;
        wing_z_axis.normalize();

        // Identify expansion 
        double expan = wing_bbox.GetLargestDist() * 1e-5;
        if ( expan < 1e-6 )
        {
            expan = 1e-6;
        }

        double height = 0.5 * wing_bbox.GetSmallestDist() + expan; // Height of spar, slightly oversized

        vec3d center = ( inside_edge_pnt + outside_edge_pnt ) / 2; // center of spar

        vec3d center_to_inner_edge = inside_edge_pnt - center;
        vec3d center_to_outer_edge = outside_edge_pnt - center;

        center_to_inner_edge.normalize();
        center_to_outer_edge.normalize();

        // Wing edge vectors (assumes linearity)
        vec3d trail_edge_vec = max_trail_edge - min_trail_edge;
        vec3d lead_edge_vec = max_lead_edge - min_lead_edge;
        vec3d inner_edge_vec = min_trail_edge - min_lead_edge;
        vec3d outer_edge_vec = max_trail_edge - max_lead_edge;

        trail_edge_vec.normalize();
        lead_edge_vec.normalize();
        inner_edge_vec.normalize();
        outer_edge_vec.normalize();

        // Determine angle between center and corner points
        vec3d center_to_le_in_vec = min_lead_edge - center;
        vec3d center_to_te_in_vec = min_trail_edge - center;
        vec3d center_to_le_out_vec = max_lead_edge - center;
        vec3d center_to_te_out_vec = max_trail_edge - center;

        center_to_le_in_vec.normalize();
        center_to_te_in_vec.normalize();
        center_to_le_out_vec.normalize();
        center_to_te_out_vec.normalize();

        // Normal vector to wing chord line
        vec3d normal_vec;

        if ( abs( inner_edge_vec.mag() - 1.0 ) <= FLT_EPSILON )
        {
            normal_vec = cross( inner_edge_vec, lead_edge_vec );
        }
        else
        {
            normal_vec = cross( outer_edge_vec, lead_edge_vec );
        }

        normal_vec.normalize();

        double alpha_0 = ( PI / 2 ) - signed_angle( inner_edge_vec, center_to_outer_edge, normal_vec ); // Initial rotation
        double theta = DEG_2_RAD * m_Theta(); // User defined angle converted to Rad

        // Get maximum angles for spar to intersect wing edges
        double max_angle_inner_le = -1 * signed_angle( center_to_inner_edge, center_to_le_in_vec, normal_vec );
        double max_angle_inner_te = -1 * signed_angle( center_to_inner_edge, center_to_te_in_vec, normal_vec );
        double max_angle_outer_le = signed_angle( center_to_le_out_vec, center_to_outer_edge, normal_vec );
        double max_angle_outer_te = signed_angle( center_to_te_out_vec, center_to_outer_edge, normal_vec );

        double beta_te = -1 * signed_angle( center_to_outer_edge, trail_edge_vec, normal_vec ); // Angle between spar and trailing edge
        double beta_le = -1 * PI + signed_angle( center_to_inner_edge, lead_edge_vec, normal_vec ); // Angle between spar and leading edge

        // Slightly oversize spar length
        double length_spar_in = expan;
        double length_spar_out = expan;
        double perp_dist;

        // Determine if the rib intersects the leading/trailing edge or inner/outer edge
        if ( theta >= 0 )
        {
            if ( theta > max_angle_inner_le )
            {
                if ( abs( sin( theta + beta_le ) ) <= FLT_EPSILON || ( min_lead_edge - max_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_in += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_lead_edge ), ( center - min_lead_edge ) ).mag() / ( min_lead_edge - max_lead_edge ).mag();
                    length_spar_in += abs( perp_dist / sin( theta + beta_le ) );
                }
            }
            else
            {
                if ( abs( cos( theta + alpha_0 ) ) <= FLT_EPSILON || ( min_trail_edge - min_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_in += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - min_lead_edge ), ( center - min_trail_edge ) ).mag() / ( min_trail_edge - min_lead_edge ).mag();
                    length_spar_in += abs( perp_dist / cos( theta + alpha_0 ) );
                }
            }

            if ( theta > max_angle_outer_te )
            {
                if ( abs( sin( theta - beta_te ) ) <= FLT_EPSILON || ( min_trail_edge - max_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_out += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_trail_edge ), ( center - min_trail_edge ) ).mag() / ( min_trail_edge - max_trail_edge ).mag();
                    length_spar_out += abs( perp_dist / sin( theta - beta_te ) );
                }
            }
            else
            {
                if ( abs( cos( theta + alpha_0 ) ) <= FLT_EPSILON || ( max_trail_edge - max_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_out += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_lead_edge ), ( center - max_trail_edge ) ).mag() / ( max_trail_edge - max_lead_edge ).mag();
                    length_spar_out += abs( perp_dist / cos( theta + alpha_0 ) );
                }
            }
        }
        else
        {
            if ( theta < max_angle_inner_te )
            {
                if ( abs( sin( theta - beta_te ) ) <= FLT_EPSILON || ( max_trail_edge - min_trail_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_in += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_trail_edge ), ( center - min_trail_edge ) ).mag() / ( max_trail_edge - min_trail_edge ).mag();
                    length_spar_in += abs( perp_dist / sin( theta - beta_te ) );
                }
            }
            else
            {
                if ( abs( cos( theta + alpha_0 ) ) <= FLT_EPSILON || ( min_trail_edge - min_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_in += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - min_lead_edge ), ( center - min_trail_edge ) ).mag() / ( min_trail_edge - min_lead_edge ).mag();
                    length_spar_in += abs( perp_dist / cos( theta + alpha_0 ) );
                }
            }

            if ( theta < max_angle_outer_le )
            {
                if ( abs( sin( theta + beta_le ) ) <= FLT_EPSILON || ( max_lead_edge - min_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_out += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_lead_edge ), ( center - min_lead_edge ) ).mag() / ( max_lead_edge - min_lead_edge ).mag();
                    length_spar_out += abs( perp_dist / sin( theta + beta_le ) );
                }
            }
            else
            {
                if ( abs( cos( theta + alpha_0 ) ) <= FLT_EPSILON || ( max_trail_edge - max_lead_edge ).mag() <= FLT_EPSILON )
                {
                    length_spar_out += length_spar_0;
                }
                else
                {
                    perp_dist = cross( ( center - max_lead_edge ), ( center - max_trail_edge ) ).mag() / ( max_trail_edge - max_lead_edge ).mag();
                    length_spar_out += abs( perp_dist / cos( theta + alpha_0 ) );
                }
            }
        }

        // Apply Rodrigues' Rotation Formula
        vec3d spar_vec_in = center_to_inner_edge * cos( theta ) + cross( center_to_inner_edge, normal_vec ) * sin( theta ) + normal_vec * dot( center_to_inner_edge, normal_vec ) * ( 1 - cos( theta ) );
        vec3d spar_vec_out = center_to_outer_edge * cos( theta ) + cross( center_to_outer_edge, normal_vec ) * sin( theta ) + normal_vec * dot( center_to_outer_edge, normal_vec ) * ( 1 - cos( theta ) );

        spar_vec_in.normalize();
        spar_vec_out.normalize();

        // Calculate final end points
        vec3d inside_edge_f = center + length_spar_in * spar_vec_in;
        vec3d outside_edge_f = center + length_spar_out * spar_vec_out;

        // Identify corners of the plane
        vec3d cornerA, cornerB, cornerC, cornerD;

        cornerA = inside_edge_f + ( height * wing_z_axis );
        cornerB = inside_edge_f - ( height * wing_z_axis );
        cornerC = outside_edge_f + ( height * wing_z_axis );
        cornerD = outside_edge_f - ( height * wing_z_axis );

        // Make Planar Surface
        m_FeaPartSurfVec[0].MakePlaneSurf( cornerA, cornerB, cornerC, cornerD );

        if ( m_FeaPartSurfVec[0].GetFlipNormal() != wing_surf.GetFlipNormal() )
        {
            m_FeaPartSurfVec[0].FlipNormal();
        }

        // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copies to be definied in UpdateSymmParts 
        for ( unsigned int j = 1; j < m_SymmIndexVec.size(); j++ )
        {
            m_FeaPartSurfVec[j] = m_FeaPartSurfVec[j - 1];
        }
    }
}

void FeaSpar::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart::UpdateDrawObjs( id, highlight );
}

//////////////////////////////////////////////////////
//===================== FeaRib =====================//
//////////////////////////////////////////////////////

FeaRib::FeaRib( string geomID, int type ) : FeaSlice( geomID, type )
{
    m_Theta.Init( "Theta", "FeaRib", this, 0.0, -90.0, 90.0 );
    m_Theta.SetDescript( "Rotation of FeaRib about axis normal to wing chord line" );
}

void FeaRib::Update()
{
    UpdateParmLimits();

    // Must call UpdateSymmIndex before
    if ( m_FeaPartSurfVec.size() > 0 )
    {
        double rotation = GetRibTotalRotation( m_RelCenterLocation(), DEG_2_RAD * m_Theta(), m_PerpendicularEdgeID );
        m_FeaPartSurfVec[0] = ComputeRibSurf( m_RelCenterLocation(), rotation );

        // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copies to be definied in UpdateSymmParts 
        for ( unsigned int j = 1; j < m_SymmIndexVec.size(); j++ )
        {
            m_FeaPartSurfVec[j] = m_FeaPartSurfVec[j - 1];
        }
    }
    // Must call UpdateSymmParts next
}

void FeaRib::UpdateParmLimits()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing )
        {
            return;
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        // Init values:
        double span = 0.0;

        // Determine wing span:
        for ( size_t i = 1; i < wing->NumXSec(); i++ )
        {
            WingSect* wing_sec = wing->GetWingSect( i );

            if ( wing_sec )
            {
                span += wing_sec->m_Span();
            }
        }

        // Set parm limits and values
        m_RelCenterLocation.SetUpperLimit( span );

        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            m_AbsCenterLocation.Set( span * m_RelCenterLocation() );
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            m_AbsCenterLocation.SetUpperLimit( span );
            m_RelCenterLocation.Set( m_AbsCenterLocation() / span );
        }
    }
}

xmlNodePtr FeaRib::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::EncodeXml( node );

    if ( fea_prt_node )
    {
        XmlUtil::AddStringNode( fea_prt_node, "PerpendicularEdgeID", m_PerpendicularEdgeID );
    }

    return fea_prt_node;
}

xmlNodePtr FeaRib::DecodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::DecodeXml( node );

    if ( fea_prt_node )
    {
        m_PerpendicularEdgeID = XmlUtil::FindString( fea_prt_node, "PerpendicularEdgeID", m_PerpendicularEdgeID );
    }

    return fea_prt_node;
}


void FeaRib::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart::UpdateDrawObjs( id, highlight );
}

////////////////////////////////////////////////////
//================= FeaFixPoint ==================//
////////////////////////////////////////////////////

FeaFixPoint::FeaFixPoint( string compID, string partID, int type ) : FeaPart( compID, type )
{
    m_ParentFeaPartID = partID;

    m_PosU.Init( "PosU", "FeaFixPoint", this, 0.0, 0.0, 1.0 );
    m_PosU.SetDescript( "Precent U Location" );

    m_PosW.Init( "PosW", "FeaFixPoint", this, 0.0, 0.0, 1.0 );
    m_PosW.SetDescript( "Precent W Location" );

    m_FixPointMassFlag.Init( "FixPointMassFlag", "FeaFixPoint", this, false, false, true );
    m_FixPointMassFlag.SetDescript( "Flag to Include Mass of FeaFixPoint" );

    m_FixPointMass.Init( "FixPointMass", "FeaFixPoint", this, 0.0, 0.0, 1e12 );
    m_FixPointMass.SetDescript( "FeaFixPoint Mass Value" );

    m_FeaPropertyIndex = -1; // No property
    m_CapFeaPropertyIndex = -1; // No property
    m_BorderFlag = false;
    m_HalfMeshFlag = false;
}

void FeaFixPoint::Update()
{
    IdentifySplitSurfIndex();

    m_FeaPartSurfVec.clear(); // FeaFixPoints are not a VspSurf
}

bool FeaFixPoint::PlaneAtYZero( piecewise_surface_type & surface ) const
{
    // PlaneAtZero is very similar to the function of the same name in SurfCore. It takes a piecewise surface
    //  as an input to determine if the surface contains points less than y = 0;
    double tol = 1.0e-6;

    piecewise_surface_type::index_type ip, jp, nupatch, nvpatch;
    nupatch = surface.number_u_patches();
    nvpatch = surface.number_v_patches();

    for ( ip = 0; ip < nupatch; ++ip )
    {
        for ( jp = 0; jp < nvpatch; ++jp )
        {
            surface_patch_type::index_type icp, jcp;
            const surface_patch_type *patch = surface.get_patch( ip, jp );

            for ( icp = 0; icp <= patch->degree_u(); ++icp )
            {
                for ( jcp = 0; jcp <= patch->degree_v(); ++jcp )
                {
                    piecewise_surface_type::point_type cp;
                    cp = patch->get_control_point( icp, jcp );
                    if ( std::abs( cp.y() ) > tol )
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool FeaFixPoint::LessThanY( piecewise_surface_type & surface, double val ) const
{
    // LessThanY is very similar to the function of the same name in SurfCore. It takes a piecewise surface
    //  as an input to determine if the surface contains points less than y = val;
    piecewise_surface_type::index_type ip, jp, nupatch, nvpatch;
    nupatch = surface.number_u_patches();
    nvpatch = surface.number_v_patches();

    for ( ip = 0; ip < nupatch; ++ip )
    {
        for ( jp = 0; jp < nvpatch; ++jp )
        {
            surface_patch_type::index_type icp, jcp;
            const surface_patch_type *patch = surface.get_patch( ip, jp );

            for ( icp = 0; icp <= patch->degree_u(); ++icp )
            {
                for ( jcp = 0; jcp <= patch->degree_v(); ++jcp )
                {
                    piecewise_surface_type::point_type cp;
                    cp = patch->get_control_point( icp, jcp );
                    if ( cp.y() > val )
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void FeaFixPoint::IdentifySplitSurfIndex()
{
    // This function is called instead of FeaPart::FetchFeaXFerSurf when the FeaPart type is FEA_FIX_POINT, since 
    //  FeaFixPoints are not surfaces. This function determines the number of split surfaces for the FeaFixPoint 
    //  Parent Surface, and determines which split surface the FeaFixPoint lies on.

    FeaPart* parent_part = StructureMgr.GetFeaPart( m_ParentFeaPartID );
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( !parent_part || !veh )
    {
        return;
    }

    vector < VspSurf > parent_surf_vec = parent_part->GetFeaPartSurfVec();

    m_SplitSurfIndex.clear();
    m_SplitSurfIndex.resize( parent_surf_vec.size() );

    for ( size_t i = 0; i < parent_surf_vec.size(); i++ )
    {
        // Get FeaFixPoint U/W values
        vec2d uw = GetUW();

        double parent_Umax = parent_surf_vec[i].GetUMax();
        double parent_Wmax = parent_surf_vec[i].GetWMax();
        double parent_Wmin = 0.0;
        double parent_Umin = 0.0;

        // Check if U/W is closed, in which case the minimum and maximum U/W will be at the same point
        bool closedU = parent_surf_vec[i].IsClosedU();
        bool closedW = parent_surf_vec[i].IsClosedW();

        // Split the parent surface
        vector< XferSurf > tempxfersurfs;
        parent_surf_vec[i].FetchXFerSurf( m_ParentGeomID, m_MainSurfIndx(), 0, tempxfersurfs );

        // Check if the UW point is on a valid patch (invalid patches are discarded in FetchXFerSurf)
        bool on_valid_patch = false;

        int num_split_surfs = tempxfersurfs.size();

        // Check if the UW point is on a valid patch (invalid patches are discarded in FetchXFerSurf)
        for ( size_t j = 0; j < num_split_surfs; j++ )
        {
            double umin = tempxfersurfs[j].m_Surface.get_u0();
            double umax = tempxfersurfs[j].m_Surface.get_umax();
            double vmin = tempxfersurfs[j].m_Surface.get_v0();
            double vmax = tempxfersurfs[j].m_Surface.get_vmax();

            if ( uw[1] >= vmin && uw[1] <= vmax && uw[0] >= umin && uw[0] <= umax )
            {
                on_valid_patch = true; // The point is on the patch
            }
        }

        for ( size_t j = 0; j < num_split_surfs; j++ )
        {
            bool addSurfFlag = true;

            if ( m_HalfMeshFlag && LessThanY( tempxfersurfs[j].m_Surface, 1e-6 ) )
            {
                addSurfFlag = false;
            }

            if ( m_HalfMeshFlag && PlaneAtYZero( tempxfersurfs[j].m_Surface ) )
            {
                addSurfFlag = false;
            }

            if ( addSurfFlag )
            {
                double umax = tempxfersurfs[j].m_Surface.get_umax();
                double umin = tempxfersurfs[j].m_Surface.get_u0();
                double vmax = tempxfersurfs[j].m_Surface.get_vmax();
                double vmin = tempxfersurfs[j].m_Surface.get_v0();

                if ( parent_surf_vec[i].IsMagicVParm() && !on_valid_patch )
                {
                    vmin -= TMAGIC;
                    vmax += TMAGIC;
                }

                // Check if FeaFixPoint is on XferSurf or border curve. Note: Not all cases of FeaFixPoints on constant UW intersection curves 
                //  are checked, since a node will always be placed there automatically
                if ( uw.x() > umin && uw.x() < umax && uw.y() > vmin && uw.y() < vmax ) // FeaFixPoint on surface
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = false;
                }
                else if ( ( uw.x() > umin && uw.x() < umax ) && ( uw.y() == vmin || uw.y() == vmax ) ) // FeaFixPoint on constant W border
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( uw.x() == umin || uw.x() == umax ) && ( uw.y() > vmin && uw.y() < vmax ) ) // FeaFixPoint on constant U border
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( uw.x() == umin || uw.x() == umax ) && ( uw.y() == vmin || uw.y() == vmax ) ) // FeaFixPoint on constant UW intersection (already a node)
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( closedU && umax == parent_Umax && uw.x() == parent_Umin ) && ( uw.y() > vmin && uw.y() < vmax ) ) // FeaFixPoint on constant closedU border 
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( closedU && umin == parent_Umin && uw.y() == parent_Umax ) && ( uw.y() > vmin && uw.y() < vmax ) ) // FeaFixPoint on constant closedU border
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( uw.x() > umin && uw.x() < umax ) && ( closedW && vmax == parent_Wmax && uw.y() == parent_Wmin ) ) // FeaFixPoint on constant closedW border
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
                else if ( ( uw.x() > umin && uw.x() < umax ) && ( closedW && vmin == parent_Wmin && uw.y() == parent_Wmax ) ) // FeaFixPoint on constant closedW border
                {
                    m_SplitSurfIndex[i].push_back( j + i * num_split_surfs );
                    m_BorderFlag = true;
                }
            }
        }
    }
}

vector < vec3d > FeaFixPoint::GetPntVec()
{
    vector < vec3d > pnt_vec;

    FeaPart* parent_part = StructureMgr.GetFeaPart( m_ParentFeaPartID );

    if ( parent_part )
    {
        vector < VspSurf > parent_surf_vec = parent_part->GetFeaPartSurfVec();
        pnt_vec.resize( parent_surf_vec.size() );

        for ( size_t i = 0; i < parent_surf_vec.size(); i++ )
        {
            pnt_vec[i] = parent_surf_vec[i].CompPnt01( m_PosU(), m_PosW() );
        }
    }
    return pnt_vec;
}

vec2d FeaFixPoint::GetUW()
{
    vec2d uw;

    FeaPart* parent_part = StructureMgr.GetFeaPart( m_ParentFeaPartID );

    if ( parent_part )
    {
        vector < VspSurf > parent_surf_vec = parent_part->GetFeaPartSurfVec();

        if ( parent_surf_vec.size() > 0 ) // Only consider main parent surface (same UW for symmetric copies)
        { 
            uw.set_x( parent_surf_vec[0].GetUMax() * m_PosU() );
            uw.set_y( parent_surf_vec[0].GetWMax() * m_PosW() );
        }
    }
    return uw;
}

xmlNodePtr FeaFixPoint::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::EncodeXml( node );

    if ( fea_prt_node )
    {
        XmlUtil::AddStringNode( fea_prt_node, "ParentFeaPartID", m_ParentFeaPartID );
    }

    return fea_prt_node;
}

xmlNodePtr FeaFixPoint::DecodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::DecodeXml( node );

    if ( fea_prt_node )
    {
        m_ParentFeaPartID = XmlUtil::FindString( fea_prt_node, "ParentFeaPartID", m_ParentFeaPartID );
    }

    return fea_prt_node;
}

void FeaFixPoint::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart* parent_part = StructureMgr.GetFeaPart( m_ParentFeaPartID );

    if ( parent_part )
    {
        vector < VspSurf > parent_surf_vec = parent_part->GetFeaPartSurfVec();

        for ( size_t i = 0; i < parent_surf_vec.size(); i++ )
        {
            m_FeaPartDO.resize( parent_surf_vec.size() );

            m_FeaPartDO[i].m_PntVec.clear();

            m_FeaPartDO[i].m_GeomID = string( "FeaFixPoint_" + std::to_string( id ) + "_" + std::to_string( i ) );
            m_FeaPartDO[i].m_Screen = DrawObj::VSP_MAIN_SCREEN;
            m_FeaPartDO[i].m_Type = DrawObj::VSP_POINTS;
            m_FeaPartDO[i].m_PointSize = 8.0;

            if ( highlight )
            {
                m_FeaPartDO[i].m_PointColor = vec3d( 1.0, 0.0, 0.0 );
            }
            else
            {
                m_FeaPartDO[i].m_PointColor = vec3d( 0.0, 0.0, 0.0 );
            }

            vec3d fixpt = parent_surf_vec[i].CompPnt01( m_PosU(), m_PosW() );
            m_FeaPartDO[i].m_PntVec.push_back( fixpt );

            m_FeaPartDO[i].m_GeomChanged = true;
        }
    }
}

bool FeaFixPoint::PtsOnPlanarPart( const vector < vec3d > & pnts )
{
    return false;
}

////////////////////////////////////////////////////
//=================== FeaSkin ====================//
////////////////////////////////////////////////////

FeaSkin::FeaSkin( string geomID, int type ) : FeaPart( geomID, type )
{
    m_IncludedElements.Set( vsp::FEA_SHELL );
    m_DrawFeaPartFlag.Set( false );

    m_RemoveSkinTrisFlag.Init( "RemoveSkinTrisFlag", "FeaSkin", this, false, false, true );
    m_RemoveSkinTrisFlag.SetDescript( "Flag to Remove Skin Triangles" );
}

void FeaSkin::Update()
{
    BuildSkinSurf();
}

void FeaSkin::BuildSkinSurf()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* currgeom = veh->FindGeom( m_ParentGeomID );

        if ( currgeom )
        {
            vector< VspSurf > surf_vec;
            currgeom->GetSurfVec( surf_vec );

            m_FeaPartSurfVec[0] = surf_vec[m_SymmIndexVec[0]];

            m_FeaPartSurfVec[0].SetSurfCfdType( vsp::CFD_NORMAL );

            // Using the primary m_FeaPartSurfVec (index 0) as a reference, calculate and transform the symmetric copies
            for ( unsigned int j = 1; j < m_SymmIndexVec.size(); j++ )
            {
                m_FeaPartSurfVec[j] = m_FeaPartSurfVec[j - 1];
            }
        }
    }
}

bool FeaSkin::PtsOnPlanarPart( const vector < vec3d > & pnts )
{
    return false;
}

////////////////////////////////////////////////////
//================= FeaDome ==================//
////////////////////////////////////////////////////

FeaDome::FeaDome( string geomID, int type ) : FeaPart( geomID, type )
{
    m_Aradius.Init( "A_Radius", "FeaDome", this, 1.0, 0.0, 1.0e12 );
    m_Aradius.SetDescript( "A (x) Radius of Dome" );

    m_Bradius.Init( "B_Radius", "FeaDome", this, 1.0, 0.0, 1.0e12 );
    m_Bradius.SetDescript( "B (y) Radius of Dome" );

    m_Cradius.Init( "C_Radius", "FeaDome", this, 1.0, 0.0, 1.0e12 );
    m_Cradius.SetDescript( "C (z) Radius of Dome" );

    m_XLoc.Init( "X_Location", "FeaDome", this, 0.0, -1.0e12, 1.0e12 );
    m_XLoc.SetDescript( "Location Along Body X Axis" );

    m_YLoc.Init( "Y_Location", "FeaDome", this, 0.0, -1.0e12, 1.0e12 );
    m_YLoc.SetDescript( "Location Along Body Y Axis" );

    m_ZLoc.Init( "Z_Location", "FeaDome", this, 0.0, -1.0e12, 1.0e12 );
    m_ZLoc.SetDescript( "Location Along Body Z Axis" );

    m_XRot.Init( "X_Rotation", "FeaDome", this, 0.0, -180, 180 );
    m_XRot.SetDescript( "Rotation About Body X Axis" );

    m_YRot.Init( "Y_Rotation", "FeaDome", this, 0.0, -180, 180 );
    m_YRot.SetDescript( "Rotation About Body Y Axis" );

    m_ZRot.Init( "Z_Rotation", "FeaDome", this, 0.0, -180, 180 );
    m_ZRot.SetDescript( "Rotation About Body Z Axis" );

    m_FlipDirectionFlag.Init( "FlipDirectionFlag", "FeaDome", this, false, false, true );
    m_FlipDirectionFlag.SetDescript( "Flag to Flip the Direction of the FeaDome" );
}

void FeaDome::Update()
{
    BuildDomeSurf();
}

typedef eli::geom::curve::piecewise_ellipse_creator<double, 3, curve_tolerance_type> piecewise_dome_creator;

void FeaDome::BuildDomeSurf()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* curr_geom = veh->FindGeom( m_ParentGeomID );

        if ( !curr_geom || m_FeaPartSurfVec.size() == 0 )
        {
            return;
        }

        m_FeaPartSurfVec[0] = VspSurf(); // Create primary VspSurf

        if ( m_IncludedElements() == vsp::FEA_SHELL || m_IncludedElements() == vsp::FEA_SHELL_AND_BEAM )
        {
            m_FeaPartSurfVec[0].SetSurfCfdType( vsp::CFD_STRUCTURE );
        }
        else
        {
            m_FeaPartSurfVec[0].SetSurfCfdType( vsp::CFD_STIFFENER );
        }

        // Build unit circle
        piecewise_curve_type c, c1, c2;
        piecewise_dome_creator pbc( 4 );
        curve_point_type origin, normal;

        origin << 0, 0, 0;
        normal << 0, 1, 0;

        pbc.set_origin( origin );
        pbc.set_x_axis_radius( 1.0 );
        pbc.set_y_axis_radius( 1.0 );

        // set circle params, make sure that entire curve goes from 0 to 4
        pbc.set_t0( 0 );
        pbc.set_segment_dt( 1, 0 );
        pbc.set_segment_dt( 1, 1 );
        pbc.set_segment_dt( 1, 2 );
        pbc.set_segment_dt( 1, 3 );

        pbc.create( c );

        c.split( c1, c2, 1.0 ); // Create half sphere

        VspCurve stringer;
        stringer.SetCurve( c1 );

        if ( m_FlipDirectionFlag() )
        {
            stringer.ReflectYZ();
            //stringer.OffsetX( 1.0 );
        }

        // Revolve to unit sphere
        m_FeaPartSurfVec[0].CreateBodyRevolution( stringer );

        //m_FeaPartSurfVec[0].OffsetX( -1.0 ); // Offset by radius

        // Scale to ellipsoid
        m_FeaPartSurfVec[0].ScaleX( m_Aradius() );
        m_FeaPartSurfVec[0].ScaleY( m_Bradius() );
        m_FeaPartSurfVec[0].ScaleZ( m_Cradius() );

        // Rotate at orgin and then translate to final location
        Matrix4d rot_mat_x, rot_mat_y, rot_mat_z;
        vec3d x_axis, y_axis, z_axis;

        x_axis.set_x( 1.0 );
        y_axis.set_y( 1.0 );
        z_axis.set_z( 1.0 );

        rot_mat_x.loadIdentity();
        rot_mat_x.rotate( DEG_2_RAD * m_XRot(), x_axis );
        m_FeaPartSurfVec[0].Transform( rot_mat_x );

        rot_mat_y.loadIdentity();
        rot_mat_y.rotate( DEG_2_RAD * m_YRot(), y_axis );
        m_FeaPartSurfVec[0].Transform( rot_mat_y );

        rot_mat_z.loadIdentity();
        rot_mat_z.rotate( DEG_2_RAD * m_ZRot(), z_axis );
        m_FeaPartSurfVec[0].Transform( rot_mat_z );

        m_FeaPartSurfVec[0].OffsetX( m_XLoc() );
        m_FeaPartSurfVec[0].OffsetY( m_YLoc() );
        m_FeaPartSurfVec[0].OffsetZ( m_ZLoc() );

        // Transform to parent geom body coordinate frame
        Matrix4d model_matrix = curr_geom->getModelMatrix();
        m_FeaPartSurfVec[0].Transform( model_matrix );

        m_FeaPartSurfVec[0].BuildFeatureLines();

        // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copies to be definied in UpdateSymmParts 
        for ( unsigned int j = 1; j < m_SymmIndexVec.size(); j++ )
        {
            m_FeaPartSurfVec[j] = m_FeaPartSurfVec[j - 1];
        }
    }
}

void FeaDome::UpdateDrawObjs( int id, bool highlight )
{
    // Two DrawObjs per Dome surface: index j correcponds to the surface (quads) and 
    //  j + 1 corresponds to the cross section feature line at u_max 

    m_FeaPartDO.clear();
    m_FeaPartDO.resize( 2 * m_FeaPartSurfVec.size() );

    for ( size_t j = 0; j < 2 * m_FeaPartSurfVec.size(); j += 2 )
    {
        m_FeaPartDO[j].m_GeomID = string( m_Name + "_" + std::to_string( id ) + "_" + std::to_string( j ) );
        m_FeaPartDO[j].m_Screen = DrawObj::VSP_MAIN_SCREEN;

        m_FeaPartDO[j + 1].m_GeomID = string( m_Name + "_" + std::to_string( id ) + "_" + std::to_string( j + 1 ) );
        m_FeaPartDO[j + 1].m_Screen = DrawObj::VSP_MAIN_SCREEN;

        if ( highlight )
        {
            m_FeaPartDO[j].m_LineColor = vec3d( 1.0, 0.0, 0.0 );
            m_FeaPartDO[j].m_LineWidth = 3.0;
            m_FeaPartDO[j + 1].m_LineColor = vec3d( 1.0, 0.0, 0.0 );
            m_FeaPartDO[j + 1].m_LineWidth = 3.0;
        }
        else
        {
            m_FeaPartDO[j].m_LineColor = vec3d( 96.0 / 255.0, 96.0 / 255.0, 96.0 / 255.0 );
            m_FeaPartDO[j].m_LineWidth = 1.0;
            m_FeaPartDO[j + 1].m_LineColor = vec3d( 96.0 / 255.0, 96.0 / 255.0, 96.0 / 255.0 );
            m_FeaPartDO[j + 1].m_LineWidth = 1.0;
        }

        // Tesselate the surface (can adjust num_u and num_v tesselation for smoothness) 
        vector < vector < vec3d > > pnts, norms, uw;
        m_FeaPartSurfVec[j / 2].Tesselate( 10, 18, pnts, norms, uw, 3, false );

        // Define quads for bulkhead surface
        m_FeaPartDO[j].m_Type = DrawObj::VSP_SHADED_QUADS;

        for ( size_t i = 0; i < pnts.size() - 1; i++ )
        {
            for ( size_t k = 0; k < pnts[i].size() - 1; k++ )
            {
                // Define quads
                vec3d corner1, corner2, corner3, corner4, norm;

                corner1 = pnts[i][k];
                corner2 = pnts[i + 1][k];
                corner3 = pnts[i + 1][k + 1];
                corner4 = pnts[i][k + 1];

                m_FeaPartDO[j].m_PntVec.push_back( corner1 );
                m_FeaPartDO[j].m_PntVec.push_back( corner2 );
                m_FeaPartDO[j].m_PntVec.push_back( corner3 );
                m_FeaPartDO[j].m_PntVec.push_back( corner4 );

                norm = norms[i][k];

                // Set normal vector
                for ( int m = 0; m < 4; m++ )
                {
                    m_FeaPartDO[j].m_NormVec.push_back( norm );
                }
            }
        }

        // Set plane color to medium glass
        for ( size_t i = 0; i < 4; i++ )
        {
            m_FeaPartDO[j].m_MaterialInfo.Ambient[i] = 0.2f;
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[i] = 0.1f;
            m_FeaPartDO[j].m_MaterialInfo.Specular[i] = 0.7f;
            m_FeaPartDO[j].m_MaterialInfo.Emission[i] = 0.0f;
        }

        if ( highlight )
        {
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[3] = 0.67f;
        }
        else
        {
            m_FeaPartDO[j].m_MaterialInfo.Diffuse[3] = 0.33f;
        }

        m_FeaPartDO[j].m_MaterialInfo.Shininess = 5.0f;

        // Add points for bulkhead cross section at u_max
        m_FeaPartDO[j + 1].m_Type = DrawObj::VSP_LINE_LOOP;

        for ( size_t i = 0; i < pnts[pnts.size() - 1].size(); i++ )
        {
            m_FeaPartDO[j + 1].m_PntVec.push_back( pnts[pnts.size() - 1][i] );
        }

        m_FeaPartDO[j].m_GeomChanged = true;
        m_FeaPartDO[j + 1].m_GeomChanged = true;
    }
}

bool FeaDome::PtsOnPlanarPart( const vector < vec3d > & pnts )
{
    return false;
}

////////////////////////////////////////////////////
//================= FeaRibArray ==================//
////////////////////////////////////////////////////

FeaRibArray::FeaRibArray( string geomID, int type ) : FeaPart( geomID, type )
{
    m_RibAbsSpacing.Init( "RibAbsSpacing", "FeaRibArray", this, 0.1, 0, 1e12 );
    m_RibAbsSpacing.SetDescript( "Absolute Spacing Between Ribs in Array" );

    m_RibRelSpacing.Init( "RibRelSpacing", "FeaRibArray", this, 0.2, 0, 1e12 );
    m_RibRelSpacing.SetDescript( "Relative Spacing Between Ribs in Array" );

    m_PositiveDirectionFlag.Init( "PositiveDirectionFlag", "FeaRibArray", this, true, false, true );
    m_PositiveDirectionFlag.SetDescript( "Flag to Increment RibArray in Positive or Negative Direction" );

    m_AbsStartLocation.Init( "AbsStartLocation", "FeaRibArray", this, 0.0, 0.0, 1e12 );
    m_AbsStartLocation.SetDescript( "Absolute Starting Location for Primary Rib" );

    m_RelStartLocation.Init( "RelStartLocation", "FeaRibArray", this, 0.0, 0.0, 1e12 );
    m_RelStartLocation.SetDescript( "Relative Starting Location for Primary Rib" );

    m_Theta.Init( "Theta", "FeaRib", this, 0.0, -90.0, 90.0 );

    m_NumRibs = 0;
}

FeaRibArray::~FeaRibArray()
{

}

void FeaRibArray::Update()
{
    CalcNumRibs();

    m_FeaPartSurfVec.clear();
    m_FeaPartSurfVec.resize( m_SymmIndexVec.size() * m_NumRibs );

    CreateFeaRibArray();
}


void FeaRibArray::CalcNumRibs()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing )
        {
            return;
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        int num_wing_sec = wing->NumXSec();

        // Init values:
        double span_f = 0.0;

        // Determine wing span:
        for ( size_t i = 1; i < num_wing_sec; i++ )
        {
            WingSect* wing_sec = wing->GetWingSect( i );

            if ( wing_sec )
            {
                span_f += wing_sec->m_Span();
            }
        }

        // Calculate number of ribs and update Parm limits and values
        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            m_AbsStartLocation.Set( m_RelStartLocation() * span_f );
            m_RibAbsSpacing.Set( m_RibRelSpacing() * span_f );

            if ( m_PositiveDirectionFlag() )
            {
                m_RibRelSpacing.SetLowerUpperLimits( ( 1 - m_RelStartLocation() ) / 100, 1.0 ); // Limit to 100 ribs
                m_NumRibs = 1 + (int)floor( ( 1 - m_RelStartLocation() ) / m_RibRelSpacing() );
            }
            else
            {
                m_RibRelSpacing.SetLowerUpperLimits( m_RelStartLocation() / 100, 1.0 ); // Limit to 100 ribs
                m_NumRibs = 1 + (int)floor( ( m_RelStartLocation() ) / m_RibRelSpacing() );
            }
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            m_RelStartLocation.Set( m_AbsStartLocation() / span_f );
            m_RibRelSpacing.Set( m_RibAbsSpacing() / span_f );

            if ( m_PositiveDirectionFlag() )
            {
                m_RibAbsSpacing.SetLowerUpperLimits( ( span_f - m_AbsStartLocation() ) / 100, span_f ); // Limit to 100 ribs
                m_NumRibs = 1 + (int)floor( ( span_f - m_AbsStartLocation() ) / m_RibAbsSpacing() );
            }
            else
            {
                m_RibAbsSpacing.SetLowerUpperLimits( m_AbsStartLocation() / 100, span_f ); // Limit to 100 ribs 
                m_NumRibs = 1 + (int)floor( span_f / m_RibAbsSpacing() );
            }
        }
    }
}

void FeaRibArray::CreateFeaRibArray()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_wing = veh->FindGeom( m_ParentGeomID );

        if ( !current_wing )
        {
            return;
        }

        WingGeom* wing = dynamic_cast<WingGeom*>( current_wing );
        assert( wing );

        vector< VspSurf > surf_vec;
        current_wing->GetSurfVec( surf_vec );
        VspSurf wing_surf = surf_vec[m_MainSurfIndx()];

        BndBox wing_bbox;
        wing_surf.GetBoundingBox( wing_bbox );

        for ( size_t i = 0; i < m_NumRibs; i++ )
        {
            double dir = 1;
            if ( !m_PositiveDirectionFlag() )
            {
                dir = -1;
            }

            // Update Rib Relative Center Location
            double rel_center_location =  m_RelStartLocation() + dir * i * m_RibRelSpacing();

            double rotation = GetRibTotalRotation( rel_center_location, DEG_2_RAD * m_Theta(), m_PerpendicularEdgeID );

            VspSurf main_rib_surf = ComputeRibSurf( rel_center_location, rotation );

            m_FeaPartSurfVec[i * m_SymmIndexVec.size()] = main_rib_surf;

            if ( m_FeaPartSurfVec[m_SymmIndexVec.size() * i].GetFlipNormal() != wing_surf.GetFlipNormal() )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i].FlipNormal();
            }

            // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copiesvto be transformed 
            for ( size_t j = 1; j < m_SymmIndexVec.size(); j++ )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j] = m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j - 1];
            }

            // Get Symmetric Translation Matrix
            vector<Matrix4d> transMats = current_wing->GetFeaTransMatVec();

            //==== Apply Transformations ====//
            for ( size_t j = 1; j < m_SymmIndexVec.size(); j++ )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].Transform( transMats[j] ); // Apply total transformation to main FeaPart surface

                if ( surf_vec[j].GetFlipNormal() != m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].GetFlipNormal() )
                {
                    m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].FlipNormal();
                }
            }
        }
    }
}

FeaRib* FeaRibArray::AddFeaRib( double center_location, int ind )
{
    FeaRib* fearib = new FeaRib( m_ParentGeomID );

    if ( fearib )
    {
        fearib->m_IncludedElements.Set( m_IncludedElements() );

        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            fearib->m_RelCenterLocation.Set( center_location );
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            fearib->m_AbsCenterLocation.Set( center_location );
        }

        fearib->m_AbsRelParmFlag.Set( m_AbsRelParmFlag() );
        fearib->m_FeaPropertyIndex.Set( m_FeaPropertyIndex() );
        fearib->m_CapFeaPropertyIndex.Set( m_CapFeaPropertyIndex() );
        fearib->m_Theta.Set( m_Theta() );
        fearib->SetPerpendicularEdgeID( m_PerpendicularEdgeID );

        fearib->SetName( string( m_Name + "_Rib_" + std::to_string( ind ) ) );

        fearib->UpdateSymmIndex();
        fearib->Update();
        fearib->UpdateSymmParts();
    }

    return fearib;
}

xmlNodePtr FeaRibArray::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::EncodeXml( node );

    if ( fea_prt_node )
    {
        XmlUtil::AddStringNode( fea_prt_node, "PerpendicularEdgeID", m_PerpendicularEdgeID );
    }

    return fea_prt_node;
}

xmlNodePtr FeaRibArray::DecodeXml( xmlNodePtr & node )
{
    xmlNodePtr fea_prt_node = FeaPart::DecodeXml( node );

    if ( fea_prt_node )
    {
        m_PerpendicularEdgeID = XmlUtil::FindString( fea_prt_node, "PerpendicularEdgeID", m_PerpendicularEdgeID );
    }

    return fea_prt_node;
}

void FeaRibArray::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart::UpdateDrawObjs( id, highlight );
}

////////////////////////////////////////////////////
//================= FeaSliceArray ==================//
////////////////////////////////////////////////////

FeaSliceArray::FeaSliceArray( string geomID, int type ) : FeaPart( geomID, type )
{
    m_SliceAbsSpacing.Init( "SliceAbsSpacing", "FeaSliceArray", this, 0.2, 0, 1e12 );
    m_SliceAbsSpacing.SetDescript( "Absolute Spacing Between Slices in Array" );

    m_SliceRelSpacing.Init( "SliceRelSpacing", "FeaSliceArray", this, 0.2, 0, 1e12 );
    m_SliceRelSpacing.SetDescript( "Relative Spacing Between Slices in Array" );

    m_PositiveDirectionFlag.Init( "PositiveDirectionFlag", "FeaSliceArray", this, true, false, true );
    m_PositiveDirectionFlag.SetDescript( "Flag to Increment SliceArray in Positive or Negative Direction" );

    m_AbsStartLocation.Init( "AbsStartLocation", "FeaSliceArray", this, 0.0, 0.0, 1e12 );
    m_AbsStartLocation.SetDescript( "Absolute Starting Location for Primary Stiffener" );

    m_RelStartLocation.Init( "RelStartLocation", "FeaSliceArray", this, 0.0, 0.0, 1e12 );
    m_RelStartLocation.SetDescript( "Relative Starting Location for Primary Stiffener" );

    m_OrientationPlane.Init( "OrientationPlane", "FeaSliceArray", this, vsp::YZ_BODY, vsp::XY_BODY, vsp::CONST_U );
    m_OrientationPlane.SetDescript( "Plane the FeaSliceArray will be Parallel to (Body or Absolute Reference Frame)" );

    m_RotationAxis.Init( "RotationAxis", "FeaSliceArray", this, vsp::X_DIR, vsp::X_DIR, vsp::Z_DIR );
    m_XRot.Init( "XRot", "FeaSliceArray", this, 0.0, -90.0, 90.0 );
    m_YRot.Init( "YRot", "FeaSliceArray", this, 0.0, -90.0, 90.0 );
    m_ZRot.Init( "ZRot", "FeaSliceArray", this, 0.0, -90.0, 90.0 );

    m_NumSlices = 0;
}

void FeaSliceArray::Update()
{
    CalcNumSlices();

    m_FeaPartSurfVec.clear(); 
    m_FeaPartSurfVec.resize( m_SymmIndexVec.size() * m_NumSlices );

    CreateFeaSliceArray();
}

void FeaSliceArray::CalcNumSlices()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_geom = veh->FindGeom( m_ParentGeomID );

        if ( !current_geom )
        {
            return;
        }

        vector< VspSurf > surf_vec;
        current_geom->GetSurfVec( surf_vec );
        VspSurf current_surf = surf_vec[m_MainSurfIndx()];

        // Determine BndBox dimensions prior to rotating and translating
        Matrix4d model_matrix = current_geom->getModelMatrix();
        model_matrix.affineInverse();

        VspSurf orig_surf = current_surf;
        orig_surf.Transform( model_matrix );

        BndBox geom_bbox;

        if ( RefFrameIsBody( m_OrientationPlane() ) )
        {
            orig_surf.GetBoundingBox( geom_bbox );
        }
        else
        {
            current_surf.GetBoundingBox( geom_bbox );
        }

        double perp_dist; // Total distance perpendicular to the FeaSlice plane

        if ( m_OrientationPlane() == vsp::XY_BODY || m_OrientationPlane() == vsp::XY_ABS )
        {
            perp_dist = geom_bbox.GetMax( 2 ) - geom_bbox.GetMin( 2 );
        }
        else if ( m_OrientationPlane() == vsp::YZ_BODY || m_OrientationPlane() == vsp::YZ_ABS )
        {
            perp_dist = geom_bbox.GetMax( 0 ) - geom_bbox.GetMin( 0 );
        }
        else if ( m_OrientationPlane() == vsp::XZ_BODY || m_OrientationPlane() == vsp::XZ_ABS )
        {
            perp_dist = geom_bbox.GetMax( 1 ) - geom_bbox.GetMin( 1 );
        }
        else if ( m_OrientationPlane() == vsp::CONST_U )
        {
            // Build conformal spine from parent geom
            ConformalSpine cs;
            cs.Build( current_surf );

            perp_dist = cs.GetSpineLength();
        }

        //Calculate number of slices and update Parm limits and values
        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            m_AbsStartLocation.Set( m_RelStartLocation() * perp_dist );
            m_SliceAbsSpacing.Set( m_SliceRelSpacing() * perp_dist );

            if ( m_PositiveDirectionFlag() )
            {
                m_SliceRelSpacing.SetLowerUpperLimits( ( 1 - m_RelStartLocation() ) / 100, 1.0 ); // Limit to 100 slices
                m_NumSlices = 1 + (int)floor( ( 1 - m_RelStartLocation() ) / m_SliceRelSpacing() );
            }
            else
            {
                m_SliceRelSpacing.SetLowerUpperLimits( m_RelStartLocation() / 100, 1.0 ); // Limit to 100 slices
                m_NumSlices = 1 + (int)floor( ( m_RelStartLocation() ) / m_SliceRelSpacing() );
            }
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            m_RelStartLocation.Set( m_AbsStartLocation() / perp_dist );
            m_SliceRelSpacing.Set( m_SliceAbsSpacing() / perp_dist );

            if ( m_PositiveDirectionFlag() )
            {
                m_SliceAbsSpacing.SetLowerUpperLimits( ( perp_dist - m_AbsStartLocation() ) / 100, perp_dist ); // Limit to 100 slices
                m_NumSlices = 1 + (int)floor( ( perp_dist - m_AbsStartLocation() ) / m_SliceAbsSpacing() );
            }
            else
            {
                m_SliceAbsSpacing.SetLowerUpperLimits( m_AbsStartLocation() / 100, perp_dist ); // Limit to 100 slices 
                m_NumSlices = 1 + (int)floor( perp_dist / m_SliceAbsSpacing() );
            }
        }
    }
}

void FeaSliceArray::CreateFeaSliceArray()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        Geom* current_geom = veh->FindGeom( m_ParentGeomID );

        if ( !current_geom )
        {
            return;
        }

        vector< VspSurf > surf_vec;
        current_geom->GetSurfVec( surf_vec );
        VspSurf current_surf = surf_vec[m_MainSurfIndx()];

        for ( size_t i = 0; i < m_NumSlices; i++ )
        {
            double dir = 1.0;
            if ( !m_PositiveDirectionFlag() )
            {
                dir = -1;
            }

            // Update Slice Relative Center Location
            double rel_center_location = m_RelStartLocation() + dir * i * m_SliceRelSpacing();

            VspSurf main_slice_surf = ComputeSliceSurf( rel_center_location, m_OrientationPlane(), m_XRot(), m_YRot(), m_ZRot() );

            m_FeaPartSurfVec[i * m_SymmIndexVec.size()] = main_slice_surf;

            if ( m_FeaPartSurfVec[m_SymmIndexVec.size() * i].GetFlipNormal() != current_surf.GetFlipNormal() )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i].FlipNormal();
            }

            // Using the primary m_FeaPartSurfVec (index 0) as a reference, setup the symmetric copiesvto be transformed 
            for ( size_t j = 1; j < m_SymmIndexVec.size(); j++ )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j] = m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j - 1];
            }

            // Get Symmetric Translation Matrix
            vector<Matrix4d> transMats = current_geom->GetFeaTransMatVec();

            //==== Apply Transformations ====//
            for ( size_t j = 1; j < m_SymmIndexVec.size(); j++ )
            {
                m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].Transform( transMats[j] ); // Apply total transformation to main FeaPart surface

                if ( surf_vec[j].GetFlipNormal() != m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].GetFlipNormal() )
                {
                    m_FeaPartSurfVec[m_SymmIndexVec.size() * i + j].FlipNormal();
                }
            }
        }
    }
}

FeaSlice* FeaSliceArray::AddFeaSlice( double center_location, int ind )
{
    FeaSlice* slice = new FeaSlice( m_ParentGeomID );

    if ( slice )
    {
        slice->m_IncludedElements.Set( m_IncludedElements() );

        if ( m_AbsRelParmFlag() == vsp::REL )
        {
            slice->m_RelCenterLocation.Set( center_location );
        }
        else if ( m_AbsRelParmFlag() == vsp::ABS )
        {
            slice->m_AbsCenterLocation.Set( center_location );
        }

        slice->m_OrientationPlane.Set( vsp::CONST_U );
        slice->m_AbsRelParmFlag.Set( m_AbsRelParmFlag() );
        slice->m_FeaPropertyIndex.Set( m_FeaPropertyIndex() );
        slice->m_CapFeaPropertyIndex.Set( m_CapFeaPropertyIndex() );

        slice->SetName( string( m_Name + "_Slice_" + std::to_string( ind ) ) );

        slice->UpdateSymmIndex();
        slice->Update();
        slice->UpdateSymmParts();
    }

    return slice;
}

void FeaSliceArray::UpdateDrawObjs( int id, bool highlight )
{
    FeaPart::UpdateDrawObjs( id, highlight );
}

////////////////////////////////////////////////////
//================= FeaProperty ==================//
////////////////////////////////////////////////////

FeaProperty::FeaProperty() : ParmContainer()
{
    m_FeaPropertyType.Init( "FeaPropertyType", "FeaProperty", this, vsp::FEA_SHELL, vsp::FEA_SHELL, vsp::FEA_BEAM );
    m_FeaPropertyType.SetDescript( "FeaElement Property Type" );

    m_Thickness.Init( "Thickness", "FeaProperty", this, 0.1, 0.0, 1.0e12 );
    m_Thickness.SetDescript( "Thickness of FeaElement" );

    m_CrossSecArea.Init( "CrossSecArea", "FeaProperty", this, 0.1, 0.0, 1.0e12 );
    m_CrossSecArea.SetDescript( "Cross-Sectional Area of FeaElement" );

    m_Izz.Init( "Izz", "FeaProperty", this, 0.1, -1.0e12, 1.0e12 );
    m_Izz.SetDescript( "Area Moment of Inertia for Bending in XY Plane of FeaElement Neutral Axis (I1)" );

    m_Iyy.Init( "Iyy", "FeaProperty", this, 0.1, -1.0e12, 1.0e12 );
    m_Iyy.SetDescript( "Area Moment of Inertia for Bending in XZ Plane of FeaElement Neutral Axis (I2)" );

    m_Izy.Init( "Izy", "FeaProperty", this, 0.0, -1.0e12, 1.0e12 );
    m_Izy.SetDescript( "Area Product of Inertia of FeaElement (I12)" );

    m_Ixx.Init( "Izz", "FeaProperty", this, 0.0, -1.0e12, 1.0e12 );
    m_Ixx.SetDescript( "Torsional Constant About FeaElement Neutral Axis (J)" );

    m_FeaMaterialIndex.Init( "FeaMaterialIndex", "FeaProperty", this, 0, 0, 1e12 );
    m_FeaMaterialIndex.SetDescript( "FeaMaterial Index for FeaProperty" );
}

FeaProperty::~FeaProperty()
{

}

void FeaProperty::ParmChanged( Parm* parm_ptr, int type )
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        veh->ParmChanged( parm_ptr, type );
    }
}

xmlNodePtr FeaProperty::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr prop_info = xmlNewChild( node, NULL, BAD_CAST "FeaPropertyInfo", NULL );

    ParmContainer::EncodeXml( prop_info );

    return prop_info;
}

xmlNodePtr FeaProperty::DecodeXml( xmlNodePtr & node )
{
    ParmContainer::DecodeXml( node );

    return node;
}

string FeaProperty::GetTypeName( )
{
    if ( m_FeaPropertyType() == vsp::FEA_SHELL )
    {
        return string( "Shell" );
    }
    if ( m_FeaPropertyType() == vsp::FEA_BEAM )
    {
        return string( "Beam" );
    }

    return string( "NONE" );
}

void FeaProperty::WriteNASTRAN( FILE* fp, int prop_id )
{
    if ( m_FeaPropertyType() == vsp::FEA_SHELL )
    {
        fprintf( fp, "PSHELL,%d,%d,%f\n", prop_id, m_FeaMaterialIndex() + 1, m_Thickness() );
    }
    if ( m_FeaPropertyType() == vsp::FEA_BEAM )
    {
        fprintf( fp, "PBEAM,%d,%d,%f,%f,%f,%f,%f\n", prop_id, m_FeaMaterialIndex() + 1, m_CrossSecArea(), m_Izz(), m_Iyy(), m_Izy(), m_Ixx() );
    }
}

void FeaProperty::WriteCalculix( FILE* fp, string ELSET )
{
    FeaMaterial* fea_mat = StructureMgr.GetFeaMaterial( m_FeaMaterialIndex() );

    if ( fea_mat )
    {
        if ( m_FeaPropertyType() == vsp::FEA_SHELL )
        {
            fprintf( fp, "*SHELL SECTION, ELSET=%s, MATERIAL=%s\n", ELSET.c_str(), fea_mat->GetName().c_str() );
            fprintf( fp, "%g\n", m_Thickness() );
        }
        if ( m_FeaPropertyType() == vsp::FEA_BEAM )
        {
            // Note: *BEAM GENERAL SECTION is supported by Abaqus but not Calculix. Calculix depends on BEAM SECTION properties
            //  where the cross-section dimensions must be explicitly defined. 
            fprintf( fp, "*BEAM GENERAL SECTION, SECTION=GENERAL, ELSET=%s, MATERIAL=%s\n", ELSET.c_str(), fea_mat->GetName().c_str() );
            fprintf( fp, "%g,%g,%g,%g,%g\n", m_CrossSecArea(), m_Izz(), m_Izy(), m_Iyy(), m_Ixx() );
        }
    }
}

////////////////////////////////////////////////////
//================= FeaMaterial ==================//
////////////////////////////////////////////////////

FeaMaterial::FeaMaterial() : ParmContainer()
{
    m_MassDensity.Init( "MassDensity", "FeaMaterial", this, 1.0, 0.0, 1.0e12 );
    m_MassDensity.SetDescript( "Mass Density of Material" );

    m_ElasticModulus.Init( "ElasticModulus", "FeaMaterial", this, 0.0, 0.0, 1.0e12 );
    m_ElasticModulus.SetDescript( "Elastic (Young's) Modulus for Material" );

    m_PoissonRatio.Init( "PoissonRatio", "FeaMaterial", this, 0.0, 0.0, 1.0 );
    m_PoissonRatio.SetDescript( "Poisson's Ratio for Material" );

    m_ThermalExpanCoeff.Init( "ThermalExpanCoeff", "FeaMaterial", this, 0.0, 0.0, 1.0e12 );
    m_ThermalExpanCoeff.SetDescript( "Thermal Expansion Coefficient for Material" );

}

FeaMaterial::~FeaMaterial()
{

}

void FeaMaterial::ParmChanged( Parm* parm_ptr, int type )
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if ( veh )
    {
        veh->ParmChanged( parm_ptr, type );
    }
}

xmlNodePtr FeaMaterial::EncodeXml( xmlNodePtr & node )
{
    xmlNodePtr mat_info = xmlNewChild( node, NULL, BAD_CAST "FeaMaterialInfo", NULL );

    ParmContainer::EncodeXml( mat_info );

    return mat_info;
}

xmlNodePtr FeaMaterial::DecodeXml( xmlNodePtr & node )
{
    ParmContainer::DecodeXml( node );

    return node;
}

void FeaMaterial::WriteNASTRAN( FILE* fp, int mat_id )
{
    fprintf( fp, "MAT1,%d,%g,%g,%g,%g,%g\n", mat_id, m_ElasticModulus(), GetShearModulus(), m_PoissonRatio(), m_MassDensity(), m_ThermalExpanCoeff() );
}

void FeaMaterial::WriteCalculix( FILE* fp, int mat_id )
{
    fprintf( fp, "*MATERIAL, NAME=%s\n", GetName().c_str() );
    fprintf( fp, "*DENSITY\n" );
    fprintf( fp, "%g\n", m_MassDensity() );
    fprintf( fp, "*ELASTIC, TYPE=ISO\n" );
    fprintf( fp, "%g,%g\n", m_ElasticModulus(), m_PoissonRatio() );
    fprintf( fp, "*EXPANSION, TYPE=ISO\n" );
    fprintf( fp, "%g\n", m_ThermalExpanCoeff() );
}

double FeaMaterial::GetShearModulus()
{
    return ( m_ElasticModulus() / ( 2 * ( m_PoissonRatio() + 1 ) ) );
}
