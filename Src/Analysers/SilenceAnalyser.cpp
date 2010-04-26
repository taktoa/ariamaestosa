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

#include "SilenceAnalyser.h"
#include "AriaCore.h"
#include "Midi/MeasureData.h"

namespace AriaMaestosa
{
    namespace SilenceAnalyser
    {
        
        void recursivelyAnalyzeSilence(RenderSilenceCallback renderSilenceCallback,
                                       const int tick, const int tick_length, const int silences_y)
        {
            
            if (tick_length<2) return;
            
            const int measure = getMeasureData()->measureAtTick(tick);
            const int end_measure = getMeasureData()->measureAtTick(tick+tick_length-1);
            const int beat = getMeasureData()->beatLengthInTicks();
            
            if (tick_length<2) return;
            
            // check if silence spawns over more than one measure
            if (measure != end_measure)
            {
                // we need to plit it in two
                const int split_tick = getMeasureData()->firstTickInMeasure(end_measure);
                
                // Check split is valid before attempting.
                if (split_tick-tick>0 and tick_length-(split_tick-tick)>0)
                {
                    recursivelyAnalyzeSilence(renderSilenceCallback, tick, split_tick-tick, silences_y);
                    recursivelyAnalyzeSilence(renderSilenceCallback, split_tick, tick_length-(split_tick-tick), silences_y);
                    return;
                }
            }
            
            if (tick < 0) return; // FIXME - find why it happens
                                  // ASSERT_E(tick,>,-1);
            
            bool dotted = false, triplet = false;
            int type = -1;
            
            int dot_delta_x = 0, dot_delta_y = 0;
            
            const float relativeLength = tick_length / (float)(getMeasureData()->beatLengthInTicks()*4);
            
            const int tick_in_measure_start = (tick) - getMeasureData()->firstTickInMeasure( getMeasureData()->measureAtTick(tick) );
            const int remaining = beat - (tick_in_measure_start % beat);
            const bool starts_on_beat = aboutEqual(remaining,0) or aboutEqual(remaining,beat);
            
            if ( aboutEqual(relativeLength, 1.0) ) type = 1;
            else if (aboutEqual(relativeLength, 3.0/2.0) and starts_on_beat){ type = 1; dotted = true; dot_delta_x = 5; dot_delta_y = 2;}
            else if (aboutEqual(relativeLength, 1.0/2.0)) type = 2;
            else if (aboutEqual(relativeLength, 3.0/4.0) and starts_on_beat){ type = 2; dotted = true; dot_delta_x = 5; dot_delta_y = 2;}
            else if (aboutEqual(relativeLength, 1.0/4.0)) type = 4;
            else if (aboutEqual(relativeLength, 1.0/3.0)){ type = 2; triplet = true; }
            else if (aboutEqual(relativeLength, 3.0/8.0) and starts_on_beat){ type = 4; dotted = true; dot_delta_x = -3; dot_delta_y = 10; }
            else if (aboutEqual(relativeLength, 1.0/8.0)) type = 8;
            else if (aboutEqual(relativeLength, 1.0/6.0)){ type = 4; triplet = true; }
            else if (aboutEqual(relativeLength, 3.0/16.0) and starts_on_beat){ type = 8; dotted = true; }
            else if (aboutEqual(relativeLength, 1.0/16.0)) type = 16;
            else if (aboutEqual(relativeLength, 1.0/12.0)){ triplet = true; type = 8; }
            else if (relativeLength < 1.0/16.0){ return; }
            else
            {
                // silence is of unknown duration. split it in a serie of silences.
                
                // start by reaching the next beat if not already done
                if (!starts_on_beat and !aboutEqual(remaining,tick_length))
                {
                    recursivelyAnalyzeSilence(renderSilenceCallback, tick, remaining, silences_y);
                    recursivelyAnalyzeSilence(renderSilenceCallback, tick+remaining, tick_length - remaining, silences_y);
                    return;
                }
                
                // split in two smaller halves. render using a simple recursion.
                float closestShorterDuration = 1;
                while(closestShorterDuration >= relativeLength) closestShorterDuration /= 2.0;
                
                const int firstLength = closestShorterDuration*(float)(getMeasureData()->beatLengthInTicks()*4);
                
                recursivelyAnalyzeSilence(renderSilenceCallback, tick, firstLength, silences_y);
                recursivelyAnalyzeSilence(renderSilenceCallback, tick + firstLength, tick_length - firstLength, silences_y);
                return;
            }
            
            renderSilenceCallback(tick_length, tick, type, silences_y, triplet, dotted, dot_delta_x, dot_delta_y);
        }
        
        // ------------------------------------------------------------------------------------------------
        
        void findSilences( RenderSilenceCallback renderSilenceCallback, INoteSource* noteSource,
                          const int first_visible_measure, const int last_visible_measure,
                          const int silences_y)
        {
            const int visible_measure_amount = last_visible_measure-first_visible_measure+1;
            bool measure_empty[visible_measure_amount+1];
            for(int i=0; i<=visible_measure_amount; i++) measure_empty[i] = true;
            
            const int visibleNoteAmount = noteSource->getNoteCount();
            if (visibleNoteAmount>0)
            {
                // by comparing the ending of the previous note to the beginning of the current note,
                // we can know if there is a silence. If multiple notes play at the same time,
                // 'previous_note_end' will contain the end position of the longest note.
                // At this point all notes will already have been split so that they do not overlap on
                // 2 measures so we don't need to care about that.
                int previous_note_end = -1;
                
                // last_note_end is similar to previous_note_end, but contains the end tick of the last note that ended
                // while previous_note_end contains the end tick of the last note that started
                int last_note_end = -1;
                
                int last_measure = -1;
                
                for (int i=0; i<visibleNoteAmount; i++)
                {       
                    const int measure = noteSource->getBeginMeasure(i);
                    ASSERT_E(measure,>=,0);                    
                    ASSERT_E(last_measure,>=,-1);
                    
                    // we switched to another measure
                    if (measure>last_measure)
                    {
                        // if the last note of previous measure does not finish at the end of the measure,
                        // we need to add a silence at the end of it
                        if (last_measure != -1 and
                            not aboutEqual(last_note_end, getMeasureData()->firstTickInMeasure(measure) ))
                        {
                            const int last_measure_id = getMeasureData()->measureAtTick(last_note_end-1);
                            const int silence_length  = getMeasureData()->lastTickInMeasure(last_measure_id) - 
                            last_note_end;
                            if (silence_length < 0) continue;
                            
                            recursivelyAnalyzeSilence(renderSilenceCallback, last_note_end,
                                                      silence_length, silences_y);
                            
                        }
                        
                        // if note is not at the very beginning of the new measure, and it's the first note of
                        // the measure, we need to add a silence before it
                        if (not aboutEqual(noteSource->getStartTick(i), getMeasureData()->firstTickInMeasure(measure) ))
                        {
                            const int silence_length = noteSource->getStartTick(i) -
                            getMeasureData()->firstTickInMeasure(measure);
                            recursivelyAnalyzeSilence(renderSilenceCallback,
                                                      getMeasureData()->firstTickInMeasure(measure),
                                                      silence_length, silences_y);
                        }
                        
                        if (last_measure != -1)
                        {
                            previous_note_end = -1; // we switched to another measure, reset and start again
                            last_note_end     = -1;
                        }
                    }
                    
                    last_measure = measure;
                    
                    // remember that this measure was not empty (only if it fits somewhere in the 'measure_empty' array)
                    if ((int)(measure-first_visible_measure) >= 0 and
                        (int)(measure-first_visible_measure) < (int)(visible_measure_amount+1))
                    {
                        if ((int)(measure-first_visible_measure) >= (int)visible_measure_amount)
                        {
                            break; // we're too far
                        }
                        measure_empty[measure-first_visible_measure] = false;
                    }
                    
                    // silences between two notes
                    const int current_begin_tick = noteSource->getStartTick(i);
                    if ( previous_note_end != -1 and !aboutEqual(previous_note_end, current_begin_tick) and
                        (current_begin_tick-previous_note_end)>0 /*and previous_note_end >= last_note_end*/)
                    {
                        recursivelyAnalyzeSilence(renderSilenceCallback, previous_note_end, current_begin_tick-previous_note_end, silences_y);
                    }
                    
                    previous_note_end = noteSource->getEndTick(i);
                    
                    // if there's multiple notes playing at the same time
                    while (i+1<visibleNoteAmount and noteSource->getStartTick(i) == noteSource->getStartTick(i+1))
                    {
                        i++;
                        previous_note_end = std::max(previous_note_end, noteSource->getEndTick(i));
                    }
                    
                    if (previous_note_end > last_note_end) last_note_end = previous_note_end;
                }//next visible note
                
                // check for silence after last note
                const int lastNoteMeasure = getMeasureData()->measureAtTick(noteSource->getStartTick(visibleNoteAmount-1));
                const unsigned int last_measure_end = getMeasureData()->lastTickInMeasure(lastNoteMeasure);
                if (!aboutEqual(last_note_end, last_measure_end ) and last_note_end>-1)
                {
                    const int silence_length = last_measure_end-last_note_end;
                    recursivelyAnalyzeSilence(renderSilenceCallback, last_note_end, silence_length, silences_y);
                }
                
                
            }// end if there are visible notes
            
            // silences in empty measures
            for (int i=0; i<visible_measure_amount; i++)
            {
                if (measure_empty[i])
                {
                    recursivelyAnalyzeSilence(renderSilenceCallback,
                                              getMeasureData()->firstTickInMeasure(first_visible_measure+i),
                                              getMeasureData()->measureLengthInTicks(first_visible_measure+i), silences_y);
                }
            }
        } // end function
        
        // ------------------------------------------------------------------------------------------------

        std::vector<SilenceInfo> g_silences_ticks;

        /**
          * @brief private utility function
          */
        void gatherSilenceCallback(const int duration, const int tick, const int type, const int silences_y,
                                   const bool triplet,  const bool dotted,
                                   const int dot_delta_x, const int dot_delta_y)
        {
            g_silences_ticks.push_back( SilenceInfo(tick, tick + duration, type, silences_y,
                                                    triplet, dotted, dot_delta_x, dot_delta_y) );
        }
        
        // ------------------------------------------------------------------------------------------------

        std::vector<SilenceInfo> findSilences(INoteSource* noteSource,
                                              const int first_visible_measure, const int last_visible_measure,
                                              const int silences_y)
        {
            g_silences_ticks.clear();
            findSilences(&gatherSilenceCallback, noteSource, first_visible_measure,
                         last_visible_measure, silences_y);
            return g_silences_ticks;
        }
        
    } // end namespace SilenceAnalyser
} // end namespace AriaMaestosa

