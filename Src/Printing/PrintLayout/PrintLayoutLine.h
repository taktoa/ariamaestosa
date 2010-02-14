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

#ifndef _PRINT_LAYOUT_LINE_H_
#define _PRINT_LAYOUT_LINE_H_

#include "Config.h"

namespace AriaMaestosa
{
    class Track;
    class LayoutElement;
    class LayoutLine;
    class PrintLayoutMeasure;
    class PrintableSequence;
    
    
    /**
     * Represents the reference to a track within a single 'LayoutLine' obejct.
     */
    //FIXME: this class contains both abstract data and absolute coords. To better fit the design
    //       it might be a good idea to split the drawing from the data (or rename the class)
    class LineTrackRef
    {
        LayoutLine* parent;
        int trackID;
    public:
        
        /**
         * Editor-specific data (each editor can override this class and set their data through it in each
         * 'LineTrackRef'.
         */
        class EditorData
        {
        public:
            virtual ~EditorData() {}
        };
    
        /** editors can put data of their own there. */
        OwnerPtr<EditorData> editor_data;
        
        /** Coordinates of that line, in print units. Will not be set initially. */
        int x0, y0, x1, y1;
        
        //int layoutElementsAmount;
        
        bool show_measure_number;

        Track* track;
        
        LineTrackRef(LayoutLine* parent, int trackID) { this->parent = parent; this->trackID = trackID; }
        int getLastNote() const;
        int getFirstNote() const;
        
        int getFirstNoteInElement(const int layoutElementID);
        int getLastNoteInElement(const int layoutElementID);
        int getFirstNoteInElement(LayoutElement* layoutElement);
        int getLastNoteInElement(LayoutElement* layoutElement);
    };
    
    /**
      * A line on a notation to print. Can contain more than one track.
      * Essentially holds some 'LayoutElement' objects (the ones that fit
      * on this line)
      */
    class LayoutLine
    {
        friend class AriaPrintable;
        
        ptr_vector<LineTrackRef> trackRenderInfo;
        ptr_vector<PrintLayoutMeasure, REF> measures;
        
        PrintableSequence* printable;
        
        std::vector<LayoutElement> layoutElements;

    public:
        /** used to store what percentage of this line's height this track should take.
          * e.g. a score with F+G clefs will need more space than a 4-string bass tab
          * so vertical space must not be divided equally
          */
        std::vector<short int> height_percent;
        
        void addLayoutElement( const LayoutElement& newElem )
        {
            layoutElements.push_back( newElem );
        }
        
        // Misc info about the coords of this track
        //FIXME: this probably should not go here. Firstly, it's not clear when the
        //       variables are set and when they're not; secondly, this class should
        //       contain only abstract layout data, if I am to follow my design
        int x0, y0, x1, y1;
        int margin_below, margin_above;
        
        LayoutLine(PrintableSequence* parent, ptr_vector<PrintLayoutMeasure, REF>& measures);
        
        int level_height;
        
        bool last_of_page;
        
        int getTrackAmount() const;
        LineTrackRef& getLineTrackRef(const int id);
        
        int getLayoutElementCount() const { return layoutElements.size(); }
        LayoutElement& getLayoutElement(const int id) { return layoutElements[id]; }
        
        int getFirstNoteInElement(const int trackID, const int layoutElementID);
        int getLastNoteInElement(const int trackID, const int layoutElementID);
        int getFirstNoteInElement(const int trackID, LayoutElement* layoutElement);
        int getLastNoteInElement(const int trackID, LayoutElement* layoutElement);
        
        int calculateHeight();
        
        PrintLayoutMeasure& getMeasureForElement(const int layoutElementID) const;
        PrintLayoutMeasure& getMeasureForElement(LayoutElement* layoutElement);
        
        int getLastMeasure() const;
        int getFirstMeasure() const;
    };
    
}

#endif