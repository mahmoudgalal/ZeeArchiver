// ExtractGUI.cpp

#include "StdAfx.h"

#include "Common/IntToString.h"
#include "Common/StringConvert.h"

#include "Windows/Error.h"
#include "Windows/FileDir.h"
#include "Windows/FileFind.h"
#include "Windows/Thread.h"

//#include "ExtractCallback.h"
//#include "../FileManager/FormatUtils.h"
//#include "../FileManager/LangUtils.h"
//#include "../FileManager/resourceGui.h"

#include "ArchiveExtractCallback.h"
#include "PropIDUtils.h"

//#include "../Explorer/MyMessages.h"

//#include "resource2.h"
//#include "ExtractRes.h"

//#include "ExtractDialog.h"
#include "ExtractGUI.h"

using namespace NWindows;

static const wchar_t *kIncorrectOutDir = L"Incorrect output directory path";

#ifndef _SFX
void ShowErrorMessage(LPCWSTR message)
{
	//LOGE("");
}
static void AddValuePair(UINT resourceID, UInt32 langID, UInt64 value, UString &s)
{
  wchar_t sz[32];
 // s += LangString(resourceID, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  s += L'\n';
}

static void AddSizePair(UINT resourceID, UInt32 langID, UInt64 value, UString &s)
{
  wchar_t sz[32];
 // s += LangString(resourceID, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  ConvertUInt64ToString(value >> 20, sz);
  s += L" (";
  s += sz;
  s += L" MB)";
  s += L'\n';
}

#endif

UString HResultToMessage(HRESULT errorCode)
{
  UString message;
  if (errorCode == E_OUTOFMEMORY)
    message = L"Out of Memory Error";//LangStringSpec(IDS_MEM_ERROR, 0x0200060B);
  else if (!NError::MyFormatMessage(errorCode, message))
    message.Empty();
  if (message.IsEmpty())
    message = L"Error";
  return message;
}

 void AddMessageToString(UString &dest, const UString &src)
{
  if (!src.IsEmpty())
  {
    if (!dest.IsEmpty())
      dest += L'\n';
    dest += src;
  }
}
class CThreadExtracting//: public CProgressThreadVirt
{

public:
  CCodecs *codecs;
  CExtractCallbackImp *ExtractCallbackSpec;
  CIntVector FormatIndices;

  UStringVector *ArchivePaths;
  UStringVector *ArchivePathsFull;
  const NWildcard::CCensorNode *WildcardCensor;
  const CExtractOptions *Options;
  CMyComPtr<IExtractCallbackUI> ExtractCallback;
  UString Title;
  UString ErrorMessage;
  UString OkMessage;
  UString OkMessageTitle;
  HRESULT Result;
  bool ThreadFinishedOK; // if there is no fatal exception
  void Process();
  HRESULT Create();
  HRESULT ProcessVirt();

};

HRESULT CThreadExtracting::ProcessVirt()
{
  CDecompressStat Stat;
  HRESULT res = DecompressArchives(codecs, FormatIndices, *ArchivePaths, *ArchivePathsFull,
      *WildcardCensor, *Options, ExtractCallbackSpec, ExtractCallback, ErrorMessage, Stat);
  #ifndef _SFX
  if (Options->TestMode && ExtractCallbackSpec->IsOK())
  {
    UString s;
    AddValuePair(/*IDS_ARCHIVES_COLON*/0, 0x02000324, Stat.NumArchives, s);
    AddValuePair(/*IDS_FOLDERS_COLON*/0, 0x02000321, Stat.NumFolders, s);
    AddValuePair(/*IDS_FILES_COLON*/0, 0x02000320, Stat.NumFiles, s);
    AddSizePair(/*IDS_SIZE_COLON*/0, 0x02000322, Stat.UnpackSize, s);
    AddSizePair(/*IDS_COMPRESSED_COLON*/0, 0x02000323, Stat.PackSize, s);

    if (Options->CalcCrc)
    {
      wchar_t temp[16];
      ConvertUInt32ToHex(Stat.CrcSum, temp);
      s += L"CRC: ";
      s += temp;
      s += L'\n';
    }

    s += L'\n';
   // s += LangString(IDS_MESSAGE_NO_ERRORS, 0x02000608);

    OkMessageTitle = Title;
    OkMessage = s;
  }
  #endif
  return res;
}
HRESULT CThreadExtracting::Create()
{
	try
	    {
	      Process();
	      ThreadFinishedOK = true;
	    }
	    catch (...) { Result = E_FAIL; }
	   // return 0;
	return S_OK;
}
void CThreadExtracting::Process()
{
  //CProgressCloser closer(ProgressDialog);
  UString m;
  try { Result = ProcessVirt(); }
  catch(const wchar_t *s) { m = s; }
  catch(const UString &s) { m = s; }
  catch(const char *s) { m = GetUnicodeString(s); }
  catch(...) { m = L"Error"; }
  if (Result != E_ABORT)
  {
    if (m.IsEmpty() && Result != S_OK)
      m = HResultToMessage(Result);
  }
  AddMessageToString(m, ErrorMessage);
  //AddMessageToString(m, ErrorPath1);
  //AddMessageToString(m, ErrorPath2);

  if (m.IsEmpty())
  {
    if (!OkMessage.IsEmpty())
    {
      //ProgressDialog.Sync.SetOkMessageTitle(OkMessageTitle);
      //ProgressDialog.Sync.SetOkMessage(OkMessage);
    }
  }
  else
  {
   // ProgressDialog.Sync.SetErrorMessage(m);
    if (Result == S_OK)
      Result = E_FAIL;
  }
}

HRESULT ExtractGUI(
    CCodecs *codecs,
    const CIntVector &formatIndices,
    UStringVector &archivePaths,
    UStringVector &archivePathsFull,
    const NWildcard::CCensorNode &wildcardCensor,
    CExtractOptions &options,
    bool showDialog,
    bool &messageWasDisplayed,
    CExtractCallbackImp *extractCallback,
    HWND hwndParent)
{
  messageWasDisplayed = false;

  CThreadExtracting extracter;
  extracter.codecs = codecs;
  extracter.FormatIndices = formatIndices;

  if (!options.TestMode)
  {
    UString outputDir = options.OutputDir;
    #ifndef UNDER_CE
    if (outputDir.IsEmpty())
      NFile::NDirectory::MyGetCurrentDirectory(outputDir);
    #endif
//    if (showDialog)
//    {
//      CExtractDialog dialog;
//      if (!NFile::NDirectory::MyGetFullPathName(outputDir, dialog.DirectoryPath))
//      {
//        ShowErrorMessage(kIncorrectOutDir);
//        messageWasDisplayed = true;
//        return E_FAIL;
//      }
//      NFile::NName::NormalizeDirPathPrefix(dialog.DirectoryPath);
//#ifndef _WIN32
//      {
//	extern const TCHAR * nameWindowToUnix(const TCHAR * lpFileName);
//        UString tmpDirectoryPath = dialog.DirectoryPath;
//	dialog.DirectoryPath = nameWindowToUnix(tmpDirectoryPath);
//      }
//#endif

      // dialog.OverwriteMode = options.OverwriteMode;
      // dialog.PathMode = options.PathMode;

//      if (dialog.Create(hwndParent) != IDOK)
//        return E_ABORT;
//      outputDir = dialog.DirectoryPath;
//      options.OverwriteMode = dialog.OverwriteMode;
//      options.PathMode = dialog.PathMode;
//      #ifndef _SFX
//      extractCallback->Password = dialog.Password;
//      extractCallback->PasswordIsDefined = !dialog.Password.IsEmpty();
//      #endif
//    }
    if (!NFile::NDirectory::MyGetFullPathName(outputDir, options.OutputDir))
    {
      ShowErrorMessage(kIncorrectOutDir);
      messageWasDisplayed = true;
      return E_FAIL;
    }
    NFile::NName::NormalizeDirPathPrefix(options.OutputDir);
    
    /*
    if(!NFile::NDirectory::CreateComplexDirectory(options.OutputDir))
    {
      UString s = GetUnicodeString(NError::MyFormatMessage(GetLastError()));
      UString s2 = MyFormatNew(IDS_CANNOT_CREATE_FOLDER,
      #ifdef LANG
      0x02000603,
      #endif
      options.OutputDir);
      MyMessageBox(s2 + UString(L'\n') + s);
      return E_FAIL;
    }
    */
  }
  
//  UString title = LangStringSpec(options.TestMode ? IDS_PROGRESS_TESTING : IDS_PROGRESS_EXTRACTING,
//      options.TestMode ? 0x02000F90: 0x02000890);
  UString title=options.TestMode ?L"Testing":L"Extracting";
  extracter.Title = title;
  extracter.ExtractCallbackSpec = extractCallback;
//  extracter.ExtractCallbackSpec->ProgressDialog = &extracter.ProgressDialog;
  extracter.ExtractCallback = extractCallback;
  extracter.ExtractCallbackSpec->Init();
//
//  extracter.ProgressDialog.CompressingMode = false;
//
  extracter.ArchivePaths = &archivePaths;
  extracter.ArchivePathsFull = &archivePathsFull;
  extracter.WildcardCensor = &wildcardCensor;
  extracter.Options = &options;
//
//  extracter.ProgressDialog.IconID = IDI_ICON;
  RINOK(extracter.Create());
//  RINOK(extracter.Create(title, hwndParent));
  messageWasDisplayed = extracter.ThreadFinishedOK ;//&
//      extracter.ProgressDialog.MessagesDisplayed;
  return extracter.Result;
}
