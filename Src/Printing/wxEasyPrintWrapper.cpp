#include "AriaCore.h"

#include "PreferencesData.h"
#include "Printing/AbstractPrintableSequence.h"
#include "Printing/wxEasyPrintWrapper.h"
#include <wx/paper.h>
#include <wx/printdlg.h>
#include <wx/dc.h>
#include <wx/graphics.h>
#include <wx/dcprint.h>
#include <wx/dcmemory.h>

#ifdef __WXMAC__

#if wxMAJOR_VERSION > 2 || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
#include <wx/osx/printdlg.h>
#else
#include <wx/mac/carbon/printdlg.h>
#endif

#endif

using namespace AriaMaestosa;

#if wxCHECK_VERSION(2,9,1) && wxUSE_GRAPHICS_CONTEXT
#include <wx/dcgraph.h>
#endif


#if 0
#pragma mark -
#pragma mark wxEasyPrintWrapper : public interface
#endif

// ----------------------------------------------------------------------------------------------------------


wxEasyPrintWrapper::wxEasyPrintWrapper(wxString title, IPrintCallback* printCallBack, float units_per_cm) : wxPrintout( title )
{
    PreferencesData* prefs = PreferencesData::getInstance();

    m_print_callback = printCallBack;
    m_page_amount    = -1; // unknown yet
    m_orient         = wxPORTRAIT;
    
    long paperTypeInt = prefs->getIntValue(SETTING_ID_PAPER_TYPE);
    
    // wxPAPER_PENV_10_ROTATED is currently the last of the enum (FIXME)
    wxPaperSize paperType = (paperTypeInt > wxPAPER_NONE and paperTypeInt <= wxPAPER_PENV_10_ROTATED ?
                             (wxPaperSize)paperTypeInt : wxPAPER_LETTER);
    m_paper_id       = paperType;
    
    m_units_per_cm   = units_per_cm;
    
    m_left_margin    = prefs->getIntValue(SETTING_ID_MARGIN_LEFT);
    m_top_margin     = prefs->getIntValue(SETTING_ID_MARGIN_TOP);
    m_right_margin   = prefs->getIntValue(SETTING_ID_MARGIN_RIGHT);
    m_bottom_margin  = prefs->getIntValue(SETTING_ID_MARGIN_BOTTOM);
    
    m_unit_width     = -1.0f;
    m_unit_height    = -1.0f;
}

// ----------------------------------------------------------------------------------------------------------

bool wxEasyPrintWrapper::performPageSetup(const bool showPageSetupDialog)
{
    std::cout << "[wxEasyPrintWrapper] Showing PAGE SETUP dialog with paper "
              << wxThePrintPaperDatabase->ConvertIdToName( m_paper_id ).mb_str() << "\n";
    
    wxPrintData printdata;
    printdata.SetPrintMode( wxPRINT_MODE_PRINTER );
    printdata.SetOrientation( m_orient ); // wxPORTRAIT, wxLANDSCAPE
    printdata.SetPaperId( m_paper_id );
    printdata.SetNoCopies(1);
    
    m_page_setup = wxPageSetupDialogData(printdata);
    m_page_setup.SetMarginTopLeft(wxPoint(m_left_margin, m_top_margin));
    m_page_setup.SetMarginBottomRight(wxPoint(m_right_margin, m_bottom_margin));
    
    if (showPageSetupDialog)
    {
        // let user change default values if he wishes to
        wxPageSetupDialog dialog( NULL,  &m_page_setup );
        if (dialog.ShowModal() == wxID_OK)
        {
            m_page_setup     = dialog.GetPageSetupData();
            m_paper_id       = m_page_setup.GetPaperId();
            m_orient         = m_page_setup.GetPrintData().GetOrientation();
            
            PreferencesData* prefs = PreferencesData::getInstance();
            prefs->setValue(SETTING_ID_PAPER_TYPE, to_wxString(m_paper_id));
            
            updateMarginMembers( true /* save preferences */ );
        }
        else
        {
            std::cout << "[wxEasyPrintWrapper] user canceled at page setup dialog" << std::endl;
            return false;
        }
    }
    
    ASSERT(m_paper_id != wxPAPER_NONE);
    ASSERT(m_page_setup.GetPaperId() != wxPAPER_NONE);

    updateCoordinateSystem();
    return true;
}

// ----------------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::updateCoordinateSystem()
{
    // ---- set-up coordinate system however we want
    // we'll use it when drawing
    wxSize paperSize = m_page_setup.GetPaperSize();                           // in millimeters
    
    float large_side = std::max(paperSize.GetWidth(), paperSize.GetHeight()); // in millimeters
    float small_side = std::min(paperSize.GetWidth(), paperSize.GetHeight()); // in millimeters
    
    //float large_side_cm = large_side / 10.0f;                                 // in centimeters
    float small_side_cm = small_side / 10.0f;                                 // in centimeters
    
    //ASSERT(large_side_cm > 0.0f);
    ASSERT(small_side_cm > 0.0f);
    
    /*
     // here i'm using an arbitrary size, use whatever you wish
     if (orient == wxPORTRAIT)
     {
     float ratio = float(largePaperSide - margin_top  - margin_bottom) /
     float(smallPaperSide - margin_left - margin_right);
     
     max_x = 680;
     max_y = max_x*ratio;
     }
     else 
     {
     float ratio = float(largePaperSide - margin_left - margin_right) /
     float(smallPaperSide - margin_top  - margin_bottom);
     
     max_y = 680;
     max_x = max_y*ratio;
     }

     */
    
    if (m_orient == wxPORTRAIT) // FIXME: http://trac.wxwidgets.org/ticket/3647 sounds relevant
    {
        float ratio = float(large_side - m_top_margin  - m_bottom_margin) /
                      float(small_side - m_left_margin - m_right_margin);
        
        m_unit_width  = (int)((small_side_cm - m_left_margin/10.f - m_right_margin/10.0f)*m_units_per_cm);
        m_unit_height = m_unit_width*ratio;
        //m_unit_height = (int)(large_side_cm * m_units_per_cm);
    }
    else
    {
        float ratio = float(large_side - m_left_margin - m_right_margin) /
                      float(small_side - m_top_margin  - m_bottom_margin);
        
        //m_unit_width  = (int)(large_side_cm * m_units_per_cm);
        m_unit_height = (int)((small_side_cm - m_top_margin/10.0f - m_bottom_margin/10.0f)* m_units_per_cm);
        m_unit_width  = m_unit_height*ratio;

    }
    
    std::cout << "[wxEasyPrintWrapper] ASKING FOR " << PRINT_VAR(m_unit_width) << PRINT_VAR(m_unit_height) << "\n";
    
    ASSERT(m_unit_width  > 0);
    ASSERT(m_unit_height > 0);
    
    //std::cout << PRINT_VAR(height) << " - " << PRINT_VAR(m_print_callback->m_title_font_height)
    //          << " - " <<  PRINT_VAR(MARGIN_UNDER_PAGE_HEADER) << std::endl;
    //std::cout << PRINT_VAR(height) << " - " << PRINT_VAR(m_print_callback->m_subtitle_font_height)
    //          << " - " <<  PRINT_VAR(MARGIN_UNDER_PAGE_HEADER) << std::endl;
}

// ----------------------------------------------------------------------------------------------------------

#ifdef __WXMAC__

void wxEasyPrintWrapper::macEditMargins(wxFrame* parentFrame)
{
    wxMacPageMarginsDialog dlg(parentFrame, &m_page_setup);
    dlg.ShowModal();
    
    m_page_setup = dlg.GetPageSetupDialogData();

    updateMarginMembers( true );
    updateCoordinateSystem();
}

#endif

// ----------------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::updateMarginMembers( bool savePreferences )
{
    m_left_margin    = m_page_setup.GetMarginTopLeft().x;
    m_top_margin     = m_page_setup.GetMarginTopLeft().y;
    m_right_margin   = m_page_setup.GetMarginBottomRight().x;
    m_bottom_margin  = m_page_setup.GetMarginBottomRight().y;
    
    PreferencesData* prefs = PreferencesData::getInstance();
    
    prefs->setValue(SETTING_ID_MARGIN_LEFT,   to_wxString(m_left_margin));
    prefs->setValue(SETTING_ID_MARGIN_TOP,    to_wxString(m_top_margin));
    prefs->setValue(SETTING_ID_MARGIN_RIGHT,  to_wxString(m_right_margin));
    prefs->setValue(SETTING_ID_MARGIN_BOTTOM, to_wxString(m_bottom_margin));
    
    if (savePreferences)
    {
        prefs->save();
    }
}

// ----------------------------------------------------------------------------------------------------------

wxPrintData wxEasyPrintWrapper::getPrintData()
{
    return m_page_setup.GetPrintData();
}

// ----------------------------------------------------------------------------------------------------------

PageSetupSummary wxEasyPrintWrapper::getPageSetupSummary() const
{
    PageSetupSummary out;
    out.paperName = wxThePrintPaperDatabase->ConvertIdToName( m_paper_id );
    //I18N: for printing (page orientation)
    out.paperOrientation = (m_orient == wxPORTRAIT ? _("Portrait") : _("Landscape"));
    //I18N: for printing (page setup)
    out.marginsLine1 = wxString::Format(_("left = %i mm, right = %i mm,"), m_left_margin, m_right_margin);
    out.marginsLine2 = wxString::Format(_("top = %i mm, bottom = %i mm"), m_top_margin, m_bottom_margin);
    return out;
}

// ----------------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::setPageCount(const int pageCount)
{
    ASSERT_E(pageCount,>,0);
    m_page_amount = pageCount;
}

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark wxEasyPrintWrapper : callbacks from wxPrintout
#endif

// ----------------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::OnBeginPrinting()
{
    std::cout << "[wxEasyPrintWrapper] onBeginPrinting :" << m_unit_width << "x" << m_unit_height << "\n";
}

// ----------------------------------------------------------------------------------------------------------

bool wxEasyPrintWrapper::OnBeginDocument(int startPage, int endPage)
{
    std::cout << "[wxEasyPrintWrapper] OnBeginDocument, from page " << startPage << " to "
              << endPage << std::endl;
    
    return wxPrintout::OnBeginDocument(startPage, endPage);
}

// ----------------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::GetPageInfo(int *minPage, int *maxPage, int *pageSelFrom, int *pageSelTo)
{
    ASSERT(m_page_amount > 0);
    
    *minPage = 1;
    *maxPage = m_page_amount;
    
    *pageSelFrom = 1;
    *pageSelTo   = m_page_amount;
}

// -----------------------------------------------------------------------------------------------------

bool wxEasyPrintWrapper::HasPage(int pageNum)
{
    ASSERT(m_page_amount > 0);
    
    if (pageNum >= 1 and pageNum <= m_page_amount) return true;
    else                                           return false;
}

// -----------------------------------------------------------------------------------------------------

bool wxEasyPrintWrapper::OnPrintPage(int pageNum)
{
    std::cout << "\n============\n[OnBeginDocument] printing page " << pageNum << "\n==============" << std::endl;
    
    // ---- setup DC with coordinate system ----
    
    ASSERT(m_unit_width  > 0);
    ASSERT(m_unit_height > 0);
    
    FitThisSizeToPageMargins(wxSize(m_unit_width, m_unit_height), m_page_setup);
    
    wxRect fitRect = GetLogicalPageMarginsRect(m_page_setup);
    
    wxCoord xoff = (fitRect.width  - m_unit_width)  / 2;
    wxCoord yoff = (fitRect.height - m_unit_height) / 2;
    OffsetLogicalOrigin(xoff, yoff);
    
    wxDC* ptr = GetDC();
    if (ptr == NULL or not ptr->IsOk())
    {
        std::cerr << "[OnBeginDocument] ERROR: DC is not Ok, interrupting printing" << std::endl;
        return false;
    }
    wxDC& dc = *ptr;
    
    
    // ---- A couple helper variables to help us during draw within paper area -----
    const int x0 = 0;
    const int y0 = 0;
    const int width  = m_unit_width;
    const int height = m_unit_height;
    const int x1 = x0 + width;
    const int y1 = y0 + height;
    
    std::cout << "[OnBeginDocument] printable area : (" << x0 << ", " << y0 << ") to ("
              << x1 << ", " << y1 << ")" << std::endl;

    
#if wxCHECK_VERSION(2,9,1) && wxUSE_GRAPHICS_CONTEXT
    wxGraphicsContext* gc = NULL;
    
    if (dynamic_cast<wxPrinterDC*>(&dc) != NULL)
    {
        gc = wxGraphicsContext::Create( dynamic_cast<wxPrinterDC&>(dc) );
    }
    else if (dynamic_cast<wxMemoryDC*>(&dc) != NULL)
    {
        gc = wxGraphicsContext::Create( dynamic_cast<wxMemoryDC&>(dc) );
    }
    wxGCDC* gcdc = new wxGCDC(gc);
    m_print_callback->printPage(pageNum, *gcdc, gc, x0, y0, x1, y1);
#else
    m_print_callback->printPage(pageNum, dc, NULL, x0, y0, x1, y1);
#endif

    
    return true;
}

// -----------------------------------------------------------------------------------------------------

void wxEasyPrintWrapper::OnEndPrinting()
{
}

// -----------------------------------------------------------------------------------------------------


