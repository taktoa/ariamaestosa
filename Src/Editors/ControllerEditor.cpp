/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include "AriaCore.h"

#include "Actions/EditAction.h"
#include "Actions/AddControlEvent.h"
#include "Actions/AddControllerSlide.h"
#include "Actions/AddTextEvent.h"
#include "Actions/DeleteControllerEvent.h"
#include "Editors/ControllerEditor.h"
#include "Editors/RelativeXCoord.h"
#include "GUI/ImageProvider.h"
#include "GUI/GraphicalSequence.h"
#include "GUI/GraphicalTrack.h"
#include "GUI/MainFrame.h"
#include "GUI/MainPane.h"
#include "Midi/CommonMidiUtils.h"
#include "Midi/MeasureData.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"
#include "Pickers/ControllerChoice.h"
#include "Pickers/InstrumentPicker.h"
#include "Renderers/RenderAPI.h"

#include <string>
#include <cmath>

#include <wx/minifram.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/textdlg.h> 
#include <wx/msgdlg.h> 

using namespace AriaMaestosa;

class ControlChangeInput : public wxMiniFrame
{
    ControllerEditor* m_parent;
    wxTextCtrl* m_input;
    wxChoice* m_units;
    
    int m_tick;
    int m_controller;
    
public:
    
    LEAK_CHECK();
    
    ControlChangeInput(ControllerEditor* parent, int tick, wxPoint where) : wxMiniFrame(NULL, wxID_ANY, wxT(""), where, wxDefaultSize,
#ifdef __WXGTK__
    wxCAPTION | wxCLOSE_BOX | wxSTAY_ON_TOP
#else
    wxCAPTION | wxCLOSE_BOX
#endif
    )
    {
        m_parent = parent;
        m_tick = tick;
        m_controller = parent->getCurrentControllerType();
        
        wxPanel* panel = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
                
        m_input = new wxTextCtrl(panel, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxWANTS_CHARS);
        m_input->SetMinSize( wxSize(100, -1) );
        sizer->Add(m_input, 0, wxALL, 3);
        
        wxString labelContents = wxT("%");
        
        if (m_controller == PSEUDO_CONTROLLER_TEMPO)
        {
            //I18N: beats per minute (tempo)
            labelContents = _("bpm");
        }
        else if (m_controller == PSEUDO_CONTROLLER_PITCH_BEND)
        {
            //I18N: as in "+2 semitones"
            labelContents = _("semitones");
        }
        else if (m_controller == 0 /* bank select */)
        {
            //I18N: as in "soundbank"
            labelContents = _("bank ID");
            m_input->SetValue("0");
        }
        
        m_units = new wxChoice(panel, wxID_ANY);
        m_units->Append(labelContents, (void*)"default");
        if (m_controller != PSEUDO_CONTROLLER_TEMPO && m_controller != 0 /* bank select */)
            m_units->Append(wxT("MIDI"), (void*)"midi");

        m_units->SetSelection(0);
        
        sizer->Add(m_units, 0, wxALL, 3);

        panel->SetSizer(sizer);
        sizer->Layout();
        sizer->SetSizeHints(panel);
        
        Fit();
        
        SetPosition( GetPosition() + wxSize(0, GetSize().GetHeight()) );
        
        Show();
        
        m_input->Connect(m_input->GetId(), wxEVT_COMMAND_TEXT_ENTER,
                         wxCommandEventHandler(ControlChangeInput::onEnter), NULL, this);
                        
        m_input->Connect(m_input->GetId(), wxEVT_CHAR,
                         wxKeyEventHandler(ControlChangeInput::onChar), NULL, this);
        
                         
        m_input->SetFocus();
        
        if (m_input->GetValue().Len() > 0)
        {
            m_input->SetSelection(-1, -1);
        }
    }
    
    void onChar(wxKeyEvent& evt)
    {
        if (evt.GetKeyCode() == WXK_ESCAPE)
        {
            Destroy();
        }
        else
        {
            evt.Skip();
        }
    }
    
    void onEnter(wxCommandEvent& evt)
    {        
        wxString units((const char*)m_units->GetClientData(m_units->GetSelection()), wxConvUTF8);
        
        wxString valueStr = m_input->GetValue();
        
        wxFloat64 value = -1;
        
        if (units == wxT("midi"))
        {
            if (not valueStr.ToDouble(&value))
            {
                wxBell();
                return;
            }
            
            if (m_controller == PSEUDO_CONTROLLER_PITCH_BEND)
            {
                if (value < -8192.0 or value > 8191.0)
                {
                    wxBell();
                    return;
                }
                
                value = ControllerEvent::fromPitchBendValue(value);
            }
            else
            {
                if (value < 0.0f or value > 127.0f)
                {
                    wxBell();
                    return;
                }
                
                // FIXME: remove this silly thing where Aria stores controller events as 127-midi
                value = 127.0 - value;
            }
        }
        else if (m_controller == 0 /* bank select */)
        {
            if (not valueStr.ToDouble(&value))
            {
                wxBell();
                return;
            }
            
            if (value < 0.0f or value > 127.0f)
            {
                wxBell();
                return;
            }
            
            // FIXME: remove this silly thing where Aria stores controller events as 127-midi
            value = 127.0 - value;
        }
        else if (m_controller == PSEUDO_CONTROLLER_TEMPO)
        {
            if (not valueStr.ToDouble(&value))
            {
                wxBell();
                return;
            }
            
            value = convertBPMToTempoBend(value);
            
            if (value < 0.0 or value > 127.0)
            {
                wxBell();
                return;
            }
        }
        else if (m_controller == PSEUDO_CONTROLLER_PITCH_BEND)
        {
            if (not valueStr.ToDouble(&value) or value < -2.0 or value > 2.0)
            {
                wxBell();
                return;
            }

            value = (value > 0.0f ? ControllerEvent::fromPitchBendValue(value/2.0*8191.0) :
                                    ControllerEvent::fromPitchBendValue(value/2.0*8192.0));
        }
        else
        {
            if (not valueStr.ToDouble(&value) or value < 0.0 or value > 100.0)
            {
                wxBell();
                return;
            }

            // map from percentage to [0, 127]
            value = 127.0 - value/100.0*127.0;
            if (value > 127.0)    value = 127.0;
            else if (value < 0.0) value = 0.0;
        }
        
        m_parent->addPreciseEvent(m_tick, value);
        Destroy();
    }
};

#if 0
#pragma mark -
#pragma mark ControllerEditor
#endif

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------

ControllerEditor::ControllerEditor(GraphicalTrack* track) : Editor(track),
    m_instrument_name( new Model<wxString>(wxT("")), true )
{
    m_mouse_is_in_editor = false;

    m_selection_begin = -1;
    m_selection_end = -1;
    m_mouse_y = -1;
    
    m_has_been_resizing = false;

    m_controller_choice = new ControllerChoice();
}

// ----------------------------------------------------------------------------------------------------------

ControllerEditor::~ControllerEditor()
{
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::renderEvents()
{
    const int area_from_y = getAreaYFrom();
    const int area_to_y   = getAreaYTo();
    const float y_zoom    = getYZoom();

    AriaRender::color(0, 0.4, 1);
    AriaRender::lineWidth(3);

    ControllerEvent* tmp;
    int previous_location = -1, previous_value = -1;

    const int currentController = m_controller_choice->getControllerID();

    const int eventAmount = m_track->getControllerEventAmount(currentController == PSEUDO_CONTROLLER_LYRICS,
                                                              Track::isTempoController(currentController) );
    
    if (currentController == PSEUDO_CONTROLLER_LYRICS or
        currentController == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE or
        currentController == 0 /* bank select */)
    {
        AriaRender::images();
    }
    
    const int x_scroll = m_gsequence->getXScrollInPixels();
    int eventsOfThisType = 0;
    
    for (int n=0; n<eventAmount; n++)
    {        
        tmp = m_track->getControllerEvent(n, currentController);
        if (tmp->getController() != currentController) continue; // only draw events of this controller
        eventsOfThisType++;
        
        const int xloc = ControllerEditor::getPositionInPixels(tmp->getTick(), m_gsequence);
        
        if (dynamic_cast<TextEvent*>(tmp) != NULL)
        {
            // we support lyrics up to 100 pixels long
            if (xloc - x_scroll > Editor::getEditorXStart() - 100)
            {
                TextEvent* evt = dynamic_cast<TextEvent*>(tmp);
                evt->getText().bind();
                AriaRender::color(0,0,0);
                
                int y;
                
                switch (n % 3)
                {
                    case 0:
                        y = (area_from_y + area_from_y + area_to_y)/3;
                        break;
                    case 1:
                        y = (area_from_y + area_to_y)/2;
                        break;
                    default:
                        y = (area_from_y + area_to_y + area_to_y)/3;
                        break;
                }
                
                AriaRender::primitives();
                
                if (tmp->getTick() >= std::min(m_selection_begin, m_selection_end) and
                    tmp->getTick() <= std::max(m_selection_begin, m_selection_end))
                {
                    AriaRender::color(0, 0.75f, 0);
                }
                else
                {
                    AriaRender::color(0.6f, 0.6f, 0.6f);
                }
                
                AriaRender::bordered_rect(xloc - x_scroll - 3, y - 12, xloc - x_scroll + 3, y - 5);
                
                AriaRender::images();
                
                
                evt->getText().render(xloc - x_scroll + 6, y);
            }
        }
        else if (currentController == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE)
        {
            const int scrolled_x = xloc - x_scroll;
            
            // we support instrument names 200 pixels long
            if (scrolled_x > Editor::getEditorXStart() - 200)
            {
                const int y = (area_from_y + area_to_y + area_to_y)/3;
                
                AriaRender::primitives();
                
                if (tmp->getTick() >= std::min(m_selection_begin, m_selection_end) and
                    tmp->getTick() <= std::max(m_selection_begin, m_selection_end))
                {
                    AriaRender::color(0, 0.75f, 0);
                }
                else
                {
                    AriaRender::color(0.6f, 0.6f, 0.6f);
                }
                
                AriaRender::bordered_rect(scrolled_x - 3, y - 6, scrolled_x + 3, y);
                
                AriaRender::images();
                const unsigned short value = tmp->getValue();
                
                m_instrument_name.getModel()->setValue(InstrumentChoice::getInstrumentName( value ));
                
                // draw instrument name
                AriaRender::color(0,0,0);
                
                m_instrument_name.bind();
                m_instrument_name.render(scrolled_x + 6, y);
            }
        }
        else if (currentController == 0) /* bank select */
        {
            const int scrolled_x = xloc - x_scroll;
            
            if (scrolled_x > Editor::getEditorXStart() - 50)
            {
                const int y = (area_from_y + area_to_y + area_to_y)/3;
                
                AriaRender::primitives();
                
                if (tmp->getTick() >= std::min(m_selection_begin, m_selection_end) and
                    tmp->getTick() <= std::max(m_selection_begin, m_selection_end))
                {
                    AriaRender::color(0, 0.75f, 0);
                }
                else
                {
                    AriaRender::color(0.6f, 0.6f, 0.6f);
                }
                
                AriaRender::bordered_rect(scrolled_x - 3, y - 6, scrolled_x + 3, y);
                
                AriaRender::images();
                const unsigned short value = (int)round(127.0f - tmp->getValue());
                m_instrument_name.getModel()->setValue(to_wxString(value));
                
                AriaRender::color(0,0,0);
                
                m_instrument_name.bind();
                m_instrument_name.render(scrolled_x + 6, y);
            }
        }
        else
        {
            // -------- Other events
            const wxFloat64 value = tmp->getValue();

            if (previous_location - x_scroll > getXEnd()) // if events are no more visible, stop drawing
                return;

            if (xloc - x_scroll > Editor::getEditorXStart())
            {
                if (previous_location != -1 and previous_value != -1)
                {
                    AriaRender::line(previous_location - x_scroll,
                                     area_from_y + previous_value*y_zoom,
                                     xloc - x_scroll,
                                     area_from_y + previous_value*y_zoom);
                }
            }
            previous_value = value;
        }
        
        previous_location = xloc;
    }// next
    
    if (currentController == PSEUDO_CONTROLLER_LYRICS or
        currentController == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE or
        currentController == 0 /* bank select */)
    {
        AriaRender::primitives();
    }
    else
    {
        // draw horizontal line from last event to end of visible area
        if (eventsOfThisType > 0)
        {
            AriaRender::line(previous_location - x_scroll,
                             area_from_y + previous_value*y_zoom,
                             getXEnd(),
                             area_from_y + previous_value*y_zoom);
        }
    }
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::render(RelativeXCoord mousex_current, int mousey_current,
                              RelativeXCoord mousex_initial, int mousey_initial, bool focus)
{
    AriaRender::beginScissors(LEFT_EDGE_X, getEditorYStart(), m_width - RIGHT_SCISSOR, m_height);
    
    // -------------------------------- background ----------------------------
    
    const int area_from_y = getAreaYFrom();
    const int area_to_y   = getAreaYTo();
    
    AriaRender::primitives();
    
    setPaleLineColor();
    AriaRender::rect(0, 0, getXEnd(), area_from_y);
    AriaRender::rect(0, area_to_y,getXEnd(), getYEnd());

    
    // -------------------------- selection ------------------------
    
    if (m_selection_begin != -1 and focus)
    {
        
        RelativeXCoord selectX1(m_selection_begin, MIDI, m_gsequence);
        RelativeXCoord selectX2(m_selection_end, MIDI, m_gsequence);
        
        AriaRender::color(0.8, 0.9, 1);
        AriaRender::rect(selectX1.getRelativeTo(WINDOW) , area_from_y,
                         selectX2.getRelativeTo(WINDOW) , area_to_y);
        
    }

    // ----------------------------------- horizontal lines -----------------------
    setPaleLineColor();
    
    // tempo
    if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_TEMPO)
    {
        // top value is 500, bottom value is 0, find where to put middle value for it to be main tempo
        const int liney = (int)(
                                area_to_y - (area_to_y-area_from_y)*((m_sequence->getTempo()-20) / 380.0)
                                );
        AriaRender::line(0, liney, getXEnd(), liney);
    }
    else
        // all others
    {
        AriaRender::line(0, (area_from_y + area_to_y)/2, getXEnd(), (area_from_y+area_to_y)/2);
        
        const int quarter_y = area_from_y + (area_to_y - area_from_y)/4;
        
        AriaRender::line(0, quarter_y, getXEnd(), quarter_y);
        
        const int three_quarters_y = area_from_y + (area_to_y - area_from_y)*3/4;
        AriaRender::line(0, three_quarters_y, getXEnd(), three_quarters_y);

    }
    
    drawVerticalMeasureLines(area_from_y, area_to_y);
    
    // ------------------------ min/max, on/off, left/right, etc. -------------------
    AriaRender::images();
    setStrongLineColor();
    
    m_controller_choice->renderTopLabel(   Editor::getEditorXStart() + 5 , area_from_y + 13);
    m_controller_choice->renderBottomLabel(Editor::getEditorXStart() + 5 , area_to_y);
    
    AriaRender::primitives();
    
    // -------------------------- draw controller events ---------------------
    renderEvents();
    
    // -------------------------- Resizing --------------------------
    if (m_graphical_track->isDragResize()) m_has_been_resizing = true;
    
    // -------------------------- Value preview --------------------------
    if (m_mouse_y != -1 and m_mouse_y >= area_from_y and m_mouse_y < area_to_y)
    {        
        const int check_y = Display::getMouseY_current();
        if (check_y > area_to_y or check_y < area_from_y)
        {
            m_mouse_y = 0;
        }
        else
        {
            int the_x = Display::getMouseX_current().getRelativeTo(WINDOW) + 8;
            int the_y = m_mouse_y + 8;
            if (the_y < area_from_y + 12)
            {
                the_y = area_from_y + 12;
            }
            
            if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_TEMPO)
            {
                int value = convertTempoBendToBPM(mouseYToValue(m_mouse_y));
                
                AriaRender::images();
                AriaRender::renderNumber((const char*)to_wxString(value).mb_str(), the_x, the_y);
                AriaRender::primitives();
            }
            else if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_PITCH_BEND)
            {
                float value = mouseYToValue(m_mouse_y);
                
                if (value >= 0.0f and value <= 127.0f)
                {
                    // bring range [-8192, 8191] to [-2, +2]
                    const double divider = (8191.0 + 8192.0)/4.0;
                    double pitchBendVal = ControllerEvent::getPitchBendValue(value)/divider;
                    
                    if (pitchBendVal == -0.0) pitchBendVal = 0.0;
                    
                    AriaRender::images();
                    AriaRender::renderNumber((const char*)to_wxString2(pitchBendVal).mb_str(), the_x, the_y);
                    AriaRender::primitives();
                }
            }
            else if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_LYRICS or
                     m_controller_choice->getControllerID() == 0 /* bank select */)
            {
                // no preview for this one
            }
            else if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE)
            {
                // no preview for this one
            }
            else
            {
                int value = round((127 - mouseYToValue(m_mouse_y))/127.0f*100.0f);
                
                AriaRender::images();
                AriaRender::renderNumber((const char*)(to_wxString(value) + wxT("%")).mb_str(), the_x, the_y);
                AriaRender::primitives();
            }
        }
    }
    
    // ----------------------- add controller events (preview) -------------------
    if (m_controller_choice->getControllerID() != PSEUDO_CONTROLLER_LYRICS and
        m_controller_choice->getControllerID() != PSEUDO_CONTROLLER_INSTRUMENT_CHANGE and
        m_controller_choice->getControllerID() != 0 /* bank select */)
    {
        const bool on_off = m_controller_choice->isOnOffController( m_controller_choice->getControllerID() );
        if (m_mouse_is_in_editor and m_selection_begin == -1 and not on_off)
        {

            AriaRender::lineWidth(3);
            AriaRender::color(0, 0.4, 1);

            if (mousey_initial >= area_from_y and mousey_initial <= area_to_y and not m_has_been_resizing)
            {
                // if out of bounds
                if (mousey_current < area_from_y) mousey_current = area_from_y;
                if (mousey_current > area_to_y)   mousey_current = area_to_y;

                int tick1 = m_track->snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI), false );
                int tick2 = m_track->snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI), false );

                if (tick2 < 0) tick2 = 0;
                if (tick1 < 0) tick1 = 0;

                AriaRender::line((tick1 - m_gsequence->getXScrollInMidiTicks()) *
                                 m_gsequence->getZoom() + Editor::getEditorXStart(), mousey_initial,
                                 (tick2 - m_gsequence->getXScrollInMidiTicks()) *
                                 m_gsequence->getZoom() + Editor::getEditorXStart(), mousey_current);
            }
        }
    }
    
    AriaRender::lineWidth(1);

    // -----------------------------------------------------------------
    // left part with names
    // -----------------------------------------------------------------

    // grey background
    if (not focus) AriaRender::color(0.4, 0.4, 0.4);
    else           AriaRender::color(0.8, 0.8, 0.8);

    AriaRender::rect(0, getEditorYStart(),
                     Editor::getEditorXStart(), getYEnd());

    // controller name
    AriaRender::images();
    AriaRender::color(0,0,0);

    m_controller_choice->renderControllerName(15, getEditorYStart() + 15);
    
    AriaRender::endScissors();

}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::mouseDown(RelativeXCoord x, const int y)
{
    int window_x = x.getRelativeTo(WINDOW);
    
    m_has_been_resizing = false;

    // prepare coords
    m_selection_begin = -1;
    m_selection_end   = -1;

    // check if user is dragging on this track
    m_mouse_is_in_editor = false;
    
    if (y < getAreaYTo() and y > getEditorYStart() and
        window_x < getWidth() - 24 and
        x.getRelativeTo(EDITOR) > -1)
    {
        m_mouse_is_in_editor = true;
    }

    // check whether we're selecting
    m_selecting = false;
    if (m_mouse_is_in_editor and Display::isSelectMorePressed())
    {
        m_selecting = true;
    }

    if (window_x < Editor::getEditorXStart() and y > getEditorYStart() and
        not m_graphical_track->isCollapsed() )
    {
        Display::popupMenu(m_controller_choice, window_x, y + 15);
        getMainFrame()->getMainPane()->SetFocus();
    }
    
    if (window_x >= Editor::getEditorXStart() and y > getEditorYStart() and
        not m_graphical_track->isCollapsed() and not m_selecting and
        m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE)
    {
        //OwnerPtr<InstrumentPicker> picker(new InstrumentPicker());
        OwnerPtr<InstrumentChoice> choice(new InstrumentChoice(-1, NULL));
        //picker->setModel(choice);
        Core::getInstrumentPicker()->setModel(choice);
        Display::popupMenu((wxMenu*)(Core::getInstrumentPicker()), window_x, y);
        
        int selection = Core::getInstrumentPicker()->getModel()->getSelectedInstrument();
        if (selection != -1)
        {
            m_track->action( new Action::AddControlEvent(m_track->snapMidiTickToGrid( x.getRelativeTo(MIDI), false ),
                                                         selection,
                                                         m_controller_choice->getControllerID()) );
            Display::render();
        }
    }
    
    if (window_x >= Editor::getEditorXStart() and y > getEditorYStart() and
        not m_graphical_track->isCollapsed() and not m_selecting and
        m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_LYRICS)
    {
        wxString defaultVal;
        
        int tick = m_track->snapMidiTickToGrid( x.getRelativeTo(MIDI), false );
        
        TextEvent* evt = m_sequence->getTextEventAt(tick, PSEUDO_CONTROLLER_LYRICS);
        if (evt != NULL) defaultVal = evt->getTextValue();
        
        wxString text = wxGetTextFromUser(wxString(_("Lyrics")) + wxT(" :"), _("Lyrics"), defaultVal);
        
        if (text.Length() > 0)
        {
            m_track->action( new Action::AddTextEvent(tick,
                                                      text,
                                                      m_controller_choice->getControllerID()) );
            Display::render();
        }
    }
    
    if (window_x >= Editor::getEditorXStart() and y > getEditorYStart() and
        not m_graphical_track->isCollapsed() and not m_selecting and
        m_controller_choice->getControllerID() == 0 /* bank select */)
    {
        wxPoint mouse(x.getRelativeTo(WINDOW), y);
        mouse = getMainFrame()->getMainPane()->ClientToScreen(mouse);
        new ControlChangeInput(this, x.getRelativeTo(MIDI), mouse);
    }
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::mouseDrag(RelativeXCoord mousex_current, const int mousey_current,
                                 RelativeXCoord mousex_initial, const int mousey_initial)
{
    if (m_mouse_is_in_editor and m_selecting)
    {
        // ------------------------ select ---------------------
        m_selection_begin = m_track->snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI), false );
        m_selection_end   = m_track->snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI), false );
    }
    m_mouse_y = mousey_current;
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::mouseUp(RelativeXCoord mousex_current, int mousey_current,
                               RelativeXCoord mousex_initial, int mousey_initial)
{

    if (m_mouse_is_in_editor)
    {
        if (m_selecting)
        {
            // ------------------------ select ---------------------

            m_selection_begin = m_track->snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI), false );
            m_selection_end   = m_track->snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI), false );

            // no selection
            if (m_selection_begin == m_selection_end)
            {
                m_selection_begin = -1;
                m_selection_end = -1;
            }
            m_selecting = false;
        }
        else
        {
            m_selection_begin = -1;
            m_selection_end   = -1;

            const int area_from_y = getAreaYFrom();
            const int area_to_y   = getAreaYTo();

            // ------------------------ add controller events ---------------------

            // if mouse is out of bounds
            if (mousex_initial.getRelativeTo(WINDOW) < Editor::getEditorXStart()) return;


            if (mousey_initial < area_from_y) return;
            if (mousey_initial > area_to_y)   return;
            if (m_graphical_track->isDragResize() or m_has_been_resizing) return;
            if (mousey_current < area_from_y) mousey_current = area_from_y;
            if (mousey_current > area_to_y)   mousey_current = area_to_y;

            if (m_controller_choice->getControllerID() == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE or
                m_controller_choice->getControllerID() == 0 /* bank select */)
            {
                return;
            }
                
            int tick1 = m_track->snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI), false );
            int tick2 = m_track->snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI), false );

            if (tick2 < 0) tick2 = 0;
            if (tick1 < 0) tick1 = 0;

            const bool on_off = m_controller_choice->isOnOffController( m_controller_choice->getControllerID() );
            if (tick1 == tick2)
            {
                float y_value = mouseYToValue(mousey_initial);

                m_track->action( new Action::AddControlEvent(tick1,
                                                             y_value,
                                                             m_controller_choice->getControllerID()) );
            }
            else if (not on_off) // on/off controllers can't have slides
            {
                if (tick1 < tick2) 
                {
                    m_track->action( new Action::AddControllerSlide(tick1,
                                                                    mouseYToValue(mousey_initial),
                                                                    tick2,
                                                                    mouseYToValue(mousey_current),
                                                                    m_controller_choice->getControllerID()) );
                }
                else
                {
                    m_track->action( new Action::AddControllerSlide(tick2,
                                                                    mouseYToValue(mousey_current),
                                                                    tick1,
                                                                    mouseYToValue(mousey_initial),
                                                                    m_controller_choice->getControllerID()) );
                }
            } // end if tick1==tick2

        } // end if meta down
    }

    m_mouse_is_in_editor=false;


    Display::render();

}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::processMouseMove(RelativeXCoord x, int y)
{
    m_mouse_y = y;
    Display::render();
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::processMouseOutsideOfMe()
{
    if (m_mouse_y != -1)
    {
        m_mouse_y = -1;
        getMainFrame()->getMainPane()->Refresh();
    }
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::rightClick(RelativeXCoord x, const int y)
{
    const int area_from_y = getAreaYFrom();
    const int area_to_y   = getAreaYTo();
    const int x_scroll = m_gsequence->getXScrollInPixels();
    int idController = m_controller_choice->getControllerID();
    
    if (idController == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE or
        idController == PSEUDO_CONTROLLER_LYRICS)
    {
#if wxMAJOR_VERSION > 2 || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
        int tick = m_track->snapMidiTickToGrid(x.getRelativeTo(MIDI), false);
        
        if (idController == PSEUDO_CONTROLLER_LYRICS)
        {
            if (m_sequence->getTextEventAt(tick, PSEUDO_CONTROLLER_LYRICS) == NULL)
            {
                return;
            }
            
            const TextEvent* eventToDelete = NULL;
            
            int event_y;
            const int event_amount = m_sequence->getTextEventAmount();
            for (int n = 0; n < event_amount; n++)
            {
                const TextEvent* evt = m_sequence->getTextEvent(n);

                if (evt->getController() != PSEUDO_CONTROLLER_LYRICS) continue;

                switch (n % 3)
                {
                    case 0:
                        event_y = (area_from_y + area_from_y + area_to_y)/3;
                        break;
                    case 1:
                        event_y = (area_from_y + area_to_y)/2;
                        break;
                    default:
                        event_y = (area_from_y + area_to_y + area_to_y)/3;
                        break;
                }
                
                const int xloc = ControllerEditor::getPositionInPixels(evt->getTick(), m_gsequence);

                int mx = x.getRelativeTo(WINDOW);
                if (mx >= xloc - x_scroll - 3 and
                    mx <= xloc - x_scroll + 5 and
                    y >= event_y - 16 and
                    y <= event_y)
                {
                    eventToDelete = evt;
                    break;
                }
            }
            
            if (eventToDelete == NULL) return;
            
            static wxMenu* menu = new wxMenu();
            static bool initialized = false;
            if (not initialized)
            {
                int id = wxNewId();
                menu->Append(id, _("Delete"));
                initialized = true;
                getMainFrame()->Bind(wxEVT_COMMAND_MENU_SELECTED, &ControllerEditor::onDeleteLyricsEvent, this, id);
            }
            m_event_tick_to_delete = eventToDelete->getTick();
            getMainFrame()->PopupMenu(menu);
        }
        else if (idController == PSEUDO_CONTROLLER_INSTRUMENT_CHANGE)
        {
            const int instruments_y = (area_from_y + area_to_y + area_to_y)/3;
            
            const int eventAmount = m_track->getControllerEventAmount();
            ControllerEvent* eventToDelete = NULL;
            for (int n=0; n<eventAmount; n++)
            {     
                ControllerEvent* evt = m_track->getControllerEvent(n, PSEUDO_CONTROLLER_INSTRUMENT_CHANGE);
                
                if (evt->getController() != PSEUDO_CONTROLLER_INSTRUMENT_CHANGE) continue;
                
                const int xloc = ControllerEditor::getPositionInPixels(evt->getTick(), m_gsequence);

                int mx = x.getRelativeTo(WINDOW);
                if (mx >= xloc - x_scroll - 3 and
                    mx <= xloc - x_scroll + 5 and
                    y >= instruments_y - 8 and
                    y <= instruments_y + 2)
                {
                    eventToDelete = evt;
                    break;
                }
            }
            
            if (eventToDelete == NULL) return;
            
            static wxMenu* menu = new wxMenu();
            static bool initialized = false;
            if (not initialized)
            {
                int id = wxNewId();
                menu->Append(id, _("Delete"));
                initialized = true;
                getMainFrame()->Bind(wxEVT_COMMAND_MENU_SELECTED, &ControllerEditor::onDeleteInstrumentEvent, this, id);
            }
            m_event_tick_to_delete = eventToDelete->getTick();
            getMainFrame()->PopupMenu(menu);
        }
#else
        return;
#endif
    }
    else if (not m_controller_choice->isOnOffController(idController))
    {
        wxPoint mouse(x.getRelativeTo(WINDOW), y);
        mouse = getMainFrame()->getMainPane()->ClientToScreen(mouse);
        new ControlChangeInput(this, x.getRelativeTo(MIDI), mouse);
    }
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::onDeleteInstrumentEvent(wxCommandEvent& evt)
{
    m_track->action(new Action::DeleteControllerEvent(m_event_tick_to_delete));
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::onDeleteLyricsEvent(wxCommandEvent& evt)
{
    m_track->action(new Action::DeleteControllerEvent(m_event_tick_to_delete));
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::mouseExited(RelativeXCoord mousex_current, int mousey_current,
                                   RelativeXCoord mousex_initial, int mousey_initial)
{
    // if mouse leaves the frame, it has the same effect as if it was released (terminate drag, terminate selection, etc.)
    this->mouseUp(mousex_current, mousey_current, mousex_initial, mousey_initial);
    m_mouse_y = -1;
    Display::render();
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::selectAll( bool selected )
{

    // Select none
    if (not selected)
    {
        m_selection_begin = -1;
        m_selection_end   = -1;
    }
    else
    {
        // Select all
        m_selection_begin = 0;
        m_selection_end   = m_sequence->getMeasureData()->getTotalTickAmount();
    }

}

// ----------------------------------------------------------------------------------------------------------

int ControllerEditor::getPositionInPixels(int tick, GraphicalSequence* gseq)
{
    return (int)(
                 tick*gseq->getZoom() + Editor::getEditorXStart()
                 );
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEditor::addPreciseEvent(int tick, wxFloat64 value)
{
    m_track->action( new Action::AddControlEvent(m_track->snapMidiTickToGrid( tick, false ),
                                                 value,
                                                 m_controller_choice->getControllerID()) );
    Display::render();
}
