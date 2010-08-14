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


#include "IO/IOUtils.h"
#include "IO/AriaFileWriter.h"
#include "Midi/Sequence.h"

#include "AriaCore.h"

#include "wx/wx.h"
#include "wx/wfstream.h"
#include "wx/stdpaths.h"
#include "wx/filename.h"
#include <iostream>

namespace AriaMaestosa {

wxString to_wxString(int i)
{
    return wxString::Format(wxT("%i"), i);
}

wxString to_wxString(float f)
{
    return wxString::Format(wxT("%f"), f);
}

void assertFailed(wxString message)
{
    std::cerr << message.mb_str() << std::endl;

#ifdef _MORE_DEBUG_CHECKS
    // trigger debugger
    int bug1 = 0;
    bug1 = 5 / bug1;
#endif

    wxMessageBox( message );
    exit(1);
}

void writeData(wxString data, wxFileOutputStream& fileout)
{
    fileout.Write( data.mb_str(), data.size());
}

wxString extract_filename(wxString filepath)
{
    return filepath.AfterLast(wxFileName::GetPathSeparator());
}

wxString extract_path(wxString str)
{
    if (str.GetChar(str.Length()) == wxFileName::GetPathSeparator())
    {
        return str.BeforeLast(wxFileName::GetPathSeparator()).BeforeLast(wxFileName::GetPathSeparator());
    }
    else return str.BeforeLast(wxFileName::GetPathSeparator());
}


wxString fromCString(const char* chars)
{
    return wxString (chars, wxConvUTF8);
}
wxString fromCString(char* chars)
{
    return wxString (chars, wxConvUTF8);
}


long atoi_u(wxString s)
{
    long value;
    if (s.ToLong(&value)) return value;
    else
    {
        std::cerr << "WARNING: Could not parse number " << s.mb_str() << std::endl;
        return -1;
    }
}


wxString showFileDialog(wxString message, wxString defaultDir,
                        wxString filename,
                        wxString wildcard, bool save)
{
    wxFileDialog* dialog = new wxFileDialog( NULL, message, defaultDir, filename, wildcard, (save?wxFD_SAVE:wxFD_OPEN));
    int answer = dialog->ShowModal();
    wxString path = dialog->GetPath();
    dialog->Hide();
    dialog->Destroy();
    if (answer != wxID_OK) return wxT("");

    return path;
}

wxString getResourcePrefix()
{
#if defined(__WXMAC__) || defined(__WXGTK__)

    static bool app_in_place = wxFileExists(
            extract_path(wxStandardPaths::Get().GetExecutablePath()) +
            wxT("/Resources/collapse.jpg") );

    if (app_in_place)
    {
        return extract_path( wxStandardPaths::Get().GetExecutablePath() ) +
                wxT("/Resources/");
    }
    else
    {
        return wxStandardPaths::Get().GetResourcesDir() + wxT("/");
    }
    
#elif defined(__WXMSW__)
    
    static bool app_in_place = wxFileExists(
            extract_path(wxStandardPaths::Get().GetExecutablePath()) +
            wxFileName::GetPathSeparator() + wxT("Resources") +
            wxFileName::GetPathSeparator() + wxT("collapse.jpg") );

    if (app_in_place)
    {
        return extract_path( wxStandardPaths::Get().GetExecutablePath() ) +
               wxFileName::GetPathSeparator() + wxT("Resources") +
               wxFileName::GetPathSeparator();
    }
    else
    {
        return wxStandardPaths::Get().GetDataDir() + wxFileName::GetPathSeparator();
    }
    
#else

#warning "Resource Prefix undefined for your platform"
return wxT("./");

#endif
}

wxString extractTitle(const wxString& inputPath)
{
    return inputPath.AfterLast(wxFileName::GetPathSeparator()).BeforeLast('.');
}
    
}
