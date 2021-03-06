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
#include "Actions/AddNote.h"
#include "Actions/EditAction.h"
//#include "GUI/GraphicalTrack.h"
//#include "Editors/GuitarEditor.h"
#include "Midi/MeasureData.h"
#include "Midi/Note.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"

#include "UnitTest.h"
#include "UnitTestUtils.h"

#include <wx/intl.h>

using namespace AriaMaestosa::Action;

// ----------------------------------------------------------------------------------------------------------

AddNote::AddNote(const int pitchID, const int startTick, const int endTick,
                 const int volume, bool select, const int string) :
//I18N: (undoable) action name
SingleTrackAction( _("add note") )
{
    m_pitch_ID   = pitchID;
    m_start_tick = startTick;
    m_end_tick   = endTick;
    m_volume     = volume;
    m_string     = string;
    m_select     = select;
}

// ----------------------------------------------------------------------------------------------------------

void AddNote::undo()
{
    Note* current_note;
    relocator.setParent(m_track);
    relocator.prepareToRelocate();
    
    while ((current_note = relocator.getNextNote()) and current_note != NULL)
    {
        const int noteAmount = m_track->getNoteAmount();
        for (int n=0; n<noteAmount; n++)
        {
            if (m_track->getNote(n) == current_note)
            {
                m_track->removeNote(n);
                break;
            }//endif
        }//next
    }//wend
}

// ----------------------------------------------------------------------------------------------------------

void AddNote::perform()
{
    ASSERT(m_track != NULL);
    
    Note* tmp_note;
    if (m_string == -1) tmp_note = new Note(m_track, m_pitch_ID, m_start_tick, m_end_tick, m_volume);
    else                tmp_note = new Note(m_track, m_pitch_ID, m_start_tick, m_end_tick, m_volume, m_string, 0);
    
    MeasureData* md = m_track->getSequence()->getMeasureData();
    const int len = md->getTotalTickAmount();
    
    const bool success = m_track->addNote( tmp_note );
    
    if (success)
    {
        if (m_select)
        {
            // select last added note
            m_track->selectNote(ALL_NOTES, false, true /* ignoreModifiers */);
            tmp_note->setSelected(true);
        }
    }
    else
    {
        delete tmp_note;
        return;
    }
    
    relocator.rememberNote( *tmp_note );
    
    tmp_note->play(true);
    
    if (m_end_tick > len)
    {        
        md->extendToTick(m_end_tick);
    }
    
    // FIXME: maybe this should be automatic?
    if (m_track->isNotationTypeEnabled(GUITAR)) m_track->updateNotesForGuitarEditor();
    
    // The note will have been inserted correctly but we also sort by pitch
    m_track->reorderNoteVector();
}

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------

using namespace AriaMaestosa;

namespace TestAddNote
{
    
    UNIT_TEST(TestAdd)
    {
        Sequence* seq = new Sequence(NULL, NULL, NULL, NULL, false);
        
        TestSequenceProvider provider(seq);
        AriaMaestosa::setCurrentSequenceProvider(&provider);
        
        Track* t = new Track(seq);
        
        // make a factory sequence to work from
        {
            OwnerPtr<Sequence::Import> import(seq->startImport());
            t->addNote_import(100 /* pitch */, 0   /* start */, 100 /* end */, 127 /* volume */, -1);
            t->addNote_import(101 /* pitch */, 101 /* start */, 200 /* end */, 127 /* volume */, -1);
            t->addNote_import(102 /* pitch */, 201 /* start */, 300 /* end */, 127 /* volume */, -1);
            t->addNote_import(103 /* pitch */, 301 /* start */, 400 /* end */, 127 /* volume */, -1);
        }
        require(t->getNoteAmount() == 4, "sanity check"); // sanity check on the way...
        
        seq->addTrack(t);
        
        // test the action
        seq->getTrack(0)->action(new AddNote(104 /* pitch */, 401 /* start */, 500 /* end */, 127 /* volume */, -1));
        
        require(t->getNoteAmount() == 5, "the number of events was increased");
        require(t->getNote(0)->getTick()    == 0,   "events were properly ordered");
        require(t->getNote(0)->getPitchID() == 100, "events were properly ordered");
        
        require(t->getNote(1)->getTick()    == 101, "events were properly ordered");
        require(t->getNote(1)->getPitchID() == 101, "events were properly ordered");
        
        require(t->getNote(2)->getTick()    == 201, "events were properly ordered");
        require(t->getNote(2)->getPitchID() == 102, "events were properly ordered");
        
        require(t->getNote(3)->getTick()    == 301, "events were properly ordered");
        require(t->getNote(3)->getPitchID() == 103, "events were properly ordered");
        
        require(t->getNote(4)->getTick()    == 401, "events were properly ordered");
        require(t->getNote(4)->getPitchID() == 104, "events were properly ordered");
        
        require(t->getNoteOffVector().size() == 5, "Note off vector was increased");
        require(t->getNoteOffVector()[0].getEndTick() == 100, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[1].getEndTick() == 200, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[2].getEndTick() == 300, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[3].getEndTick() == 400, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[4].getEndTick() == 500, "Note off vector is properly ordered");
        
        // Now test undo
        seq->undo();
        
        require(t->getNoteAmount() == 4, "the number of events was restored on undo");
        require(t->getNote(0)->getTick()    == 0,   "events were properly ordered");
        require(t->getNote(0)->getPitchID() == 100, "events were properly ordered");
        
        require(t->getNote(1)->getTick()    == 101, "events were properly ordered");
        require(t->getNote(1)->getPitchID() == 101, "events were properly ordered");
        
        require(t->getNote(2)->getTick()    == 201, "events were properly ordered");
        require(t->getNote(2)->getPitchID() == 102, "events were properly ordered");
        
        require(t->getNote(3)->getTick()    == 301, "events were properly ordered");
        require(t->getNote(3)->getPitchID() == 103, "events were properly ordered");
        
        delete seq;
    }
    
    // ----------------------------------------------------------------------------------------------------------
    
    UNIT_TEST(TestInsert)
    {            
        Sequence* seq = new Sequence(NULL, NULL, NULL, NULL, false);
        
        TestSequenceProvider provider(seq);
        AriaMaestosa::setCurrentSequenceProvider(&provider);
        
        Track* t = new Track(seq);

        // make a factory sequence to work from
        {
            OwnerPtr<Sequence::Import> import(seq->startImport());
            t->addNote_import(100 /* pitch */, 0   /* start */, 100 /* end */, 127 /* volume */, -1);
            t->addNote_import(101 /* pitch */, 101 /* start */, 200 /* end */, 127 /* volume */, -1);
            t->addNote_import(102 /* pitch */, 201 /* start */, 300 /* end */, 127 /* volume */, -1);
            t->addNote_import(103 /* pitch */, 301 /* start */, 400 /* end */, 127 /* volume */, -1);
        }
        require(t->getNoteAmount() == 4, "sanity check"); // sanity check on the way...
        
        seq->addTrack(t);
        
        // test the action
        seq->getTrack(0)->action(new AddNote(104 /* pitch */, 150 /* start */, 250 /* end */, 127 /* volume */, -1));
        
        require(t->getNoteAmount() == 5, "the number of events was increased");
        require(t->getNote(0)->getTick()    == 0,   "events were properly ordered");
        require(t->getNote(0)->getPitchID() == 100, "events were properly ordered");
        
        require(t->getNote(1)->getTick()    == 101, "events were properly ordered");
        require(t->getNote(1)->getPitchID() == 101, "events were properly ordered");
        
        require(t->getNote(2)->getTick()    == 150, "events were properly ordered");
        require(t->getNote(2)->getPitchID() == 104, "events were properly ordered");
        
        require(t->getNote(3)->getTick()    == 201, "events were properly ordered");
        require(t->getNote(3)->getPitchID() == 102, "events were properly ordered");
        
        require(t->getNote(4)->getTick()    == 301, "events were properly ordered");
        require(t->getNote(4)->getPitchID() == 103, "events were properly ordered");
        
        require(t->getNoteOffVector().size() == 5, "Note off vector was increased");
        require(t->getNoteOffVector()[0].getEndTick() == 100, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[1].getEndTick() == 200, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[2].getEndTick() == 250, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[3].getEndTick() == 300, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[4].getEndTick() == 400, "Note off vector is properly ordered");
        
        // Now test undo
        seq->undo();
        
        require(t->getNoteAmount() == 4, "the number of events was restored on undo");
        require(t->getNote(0)->getTick()    == 0,   "events were properly ordered");
        require(t->getNote(0)->getPitchID() == 100, "events were properly ordered");
        
        require(t->getNote(1)->getTick()    == 101, "events were properly ordered");
        require(t->getNote(1)->getPitchID() == 101, "events were properly ordered");
        
        require(t->getNote(2)->getTick()    == 201, "events were properly ordered");
        require(t->getNote(2)->getPitchID() == 102, "events were properly ordered");
        
        require(t->getNote(3)->getTick()    == 301, "events were properly ordered");
        require(t->getNote(3)->getPitchID() == 103, "events were properly ordered");

        require(t->getNoteOffVector().size() == 4, "Note off vector was restored on undo");
        require(t->getNoteOffVector()[0].getEndTick() == 100, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[1].getEndTick() == 200, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[2].getEndTick() == 300, "Note off vector is properly ordered");
        require(t->getNoteOffVector()[3].getEndTick() == 400, "Note off vector is properly ordered");
        
        delete seq;
    }
    
}
// ----------------------------------------------------------------------------------------------------------
