/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Windows main function of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
  This file contains the code in the qtmain library for WinRT.
  qtmain contains the WinRT startup code and is required for
  linking to the Qt DLL.

  When a Windows application starts, the WinMain function is
  invoked. WinMain creates the WinRT application which in turn
  calls the main entry point.
*/

extern "C" int main(int, char **);

#include <wrl.h>
#include <Windows.ApplicationModel.core.h>

class AppxContainer : public Microsoft::WRL::RuntimeClass<ABI::Windows::ApplicationModel::Core::IFrameworkView>
{
public:
    AppxContainer() { }

    // IFrameworkView Methods
    HRESULT __stdcall Initialize(ABI::Windows::ApplicationModel::Core::ICoreApplicationView *) { return S_OK; }
    HRESULT __stdcall SetWindow(ABI::Windows::UI::Core::ICoreWindow *) { return S_OK; }
    HRESULT __stdcall Load(HSTRING) { return S_OK; }
    HRESULT __stdcall Run() {
        // TODO: pass actual arguments from system
        char *argv[] = {"app.exe", "-platform", "winrt", "-platformpluginpath", "."};
        int argc = ARRAYSIZE(argv);
        main(argc, argv);
        return S_OK;
    }
    HRESULT __stdcall Uninitialize() { return S_OK; }
};

class AppxViewSource : public Microsoft::WRL::RuntimeClass<ABI::Windows::ApplicationModel::Core::IFrameworkViewSource>
{
public:
    AppxViewSource() { }
    HRESULT __stdcall CreateView(ABI::Windows::ApplicationModel::Core::IFrameworkView **frameworkView) {
        return (*frameworkView = Microsoft::WRL::Make<AppxContainer>().Detach()) ? S_OK : E_OUTOFMEMORY;
    }
};

// Main entry point for Appx containers
int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    if (FAILED(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED)))
        return 1;

    Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::Core::ICoreApplication> appFactory;
    if (FAILED(Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HString::MakeReference(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication).Get(), &appFactory)))
        return 2;

    // TODO: pass arguments from command line
    // Note that GetCommandLine() is not listed as a supported Windows Store API, so the arguments may need to come from WinMain.
    return appFactory->Run(Microsoft::WRL::Make<AppxViewSource>().Get());
}
