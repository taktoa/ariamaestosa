
#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/file.h>

#include "Midi/Sequence.h"
#include "AriaCore.h"

#include "Midi/MeasureData.h"
#include "Editors/ScoreEditor.h"
#include "Editors/ScoreAnalyser.h"
#include "IO/IOUtils.h"
#include "Printing/ScorePrint.h"
#include "Printing/PrintingBase.h"
#include "Printing/PrintLayout.h"

namespace AriaMaestosa
{
    
    
ScorePrintable::ScorePrintable(Track* track) : EditorPrintable()
{

}

ScorePrintable::~ScorePrintable()
{
}

// FIXME - F key, notes above/below score, etc... --> variable score height needed
void ScorePrintable::drawLine(LayoutLine& line, wxDC& dc, const int x0, const int y0, const int x1, const int y1, bool show_measure_number)
{
    Track* track = line.getTrack();
    ScoreEditor* scoreEditor = track->graphics->scoreEditor;
    ScoreMidiConverter* converter = scoreEditor->getScoreMidiConverter();
    
    const int middleC = converter->getMiddleCLevel();
    const int lineAmount = 5;
    
    // draw score background (lines)
    dc.SetPen(  wxPen( wxColour(125,125,125), 1 ) );
    
    const float lineHeight = (float)(y1 - y0) / (float)(lineAmount-1);
    const int headRadius = (int)round((float)lineHeight*0.35);
    
    for(int s=0; s<lineAmount; s++)
    {
        const int y = (int)round(y0 + lineHeight*s);
        dc.DrawLine(x0, y, x1, y);
    }
    
    beginLine(&dc, &line, x0, y0, x1, y1, show_measure_number);
    
    std::vector<NoteRenderInfo> noteRenderInfo;
    
    // iterate through layout elements to collect notes in the vector
    // so ScoreAnalyser can prepare the score
    LayoutElement* currentElement;
    while((currentElement = getNextElement()) and (currentElement != NULL))
    {
        if(currentElement->type != SINGLE_MEASURE) continue;
        
        const int firstNote = line.getFirstNoteInElement(currentElement);
        const int lastNote = line.getLastNoteInElement(currentElement);
        
        // for layout elements containing notes, render them
        for(int n=firstNote; n<lastNote; n++)
        {
            int note_sign;
            const int noteLevel = converter->noteToLevel(track->getNote(n), &note_sign);
            
            if(noteLevel == -1) continue;
            const int noteLength = track->getNoteEndInMidiTicks(n) - track->getNoteStartInMidiTicks(n);
            const int tick = track->getNoteStartInMidiTicks(n);
            
            const int note_x = tickToX(tick);
            
			NoteRenderInfo currentNote(tick, note_x, noteLevel, noteLength, note_sign,
                                       track->isNoteSelected(n), track->getNotePitchID(n));
			addToVector(currentNote, noteRenderInfo, false);
            
        }
        
    }//next element
    
    // analyse notes to know how to build the score
    analyseNoteInfo(noteRenderInfo, scoreEditor);
    
    const int noteAmount = noteRenderInfo.size();
    for(int i=0; i<noteAmount; i++)
    {
        // draw note head
        dc.SetPen(  wxPen( wxColour(0,0,0), 2 ) );

        if(noteRenderInfo[i].hollow_head) dc.SetBrush( *wxTRANSPARENT_BRUSH );
        else dc.SetBrush( *wxBLACK_BRUSH );
        
        wxPoint headLocation( noteRenderInfo[i].x + headRadius + headRadius/1.5,
                              y0 + 1 + lineHeight*0.5*(noteRenderInfo[i].getBaseLevel() - middleC + 10));
        dc.DrawCircle( headLocation, headRadius );
    }
    
}


}
