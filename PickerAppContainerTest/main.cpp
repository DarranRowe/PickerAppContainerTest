#include <Windows.h>
#include <inspectable.h>
#include <Unknwn.h>
#include <DispatcherQueue.h>
#include <shobjidl.h>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>

LRESULT CALLBACK wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_CREATE:
	{
		CREATESTRUCTW &cs = *reinterpret_cast<CREATESTRUCTW *>(lparam);

		HWND button_handle = CreateWindowExW(0, L"Button", L"ClickMe", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 50, 50, 250, 50, wnd, reinterpret_cast<HMENU>(101), cs.hInstance, nullptr);
		if (!button_handle)
		{
			return -1;
		}

		SetWindowLongPtrW(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(button_handle));
		return 0;
	}
	case WM_CLOSE:
	{
		DestroyWindow(wnd);
		return 0;
	}
	case WM_DESTROY:
	{
		HWND button_handle = reinterpret_cast<HWND>(GetWindowLongPtrW(wnd, GWLP_USERDATA));
		SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
		DestroyWindow(button_handle);
		PostQuitMessage(0);
		return 0;
	}
	case WM_COMMAND:
	{
		WORD cid = LOWORD(wparam);
		WORD cnm = HIWORD(wparam);
		HWND chnd = reinterpret_cast<HWND>(lparam);

		switch (cid)
		{
		case 101:
		{
			if (cnm == BN_CLICKED)
			{
				auto dq = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

				dq.TryEnqueue([wnd]() -> winrt::Windows::Foundation::IAsyncOperation<winrt::hresult>
					{
						winrt::Windows::Storage::Pickers::FileOpenPicker fop;

						fop.FileTypeFilter().Append(L".txt");

						auto iiww = fop.as<IInitializeWithWindow>();

						HRESULT hr = iiww->Initialize(wnd);
						if (FAILED(hr))
						{
							std::unique_ptr<wchar_t[]> output_buffer = std::make_unique<wchar_t[]>(501);
							swprintf(output_buffer.get(), 500, L"IInitializeWithWindow::Initialize failed. HRESULT: 0x%x\r\n", hr);
							OutputDebugStringW(output_buffer.get());
							co_return hr;
						}

						auto file_result = co_await fop.PickSingleFileAsync();

						co_return S_OK;
					});
			}
		}
		}

		return 0;
	}
	default:
	{
		return DefWindowProcW(wnd, msg, wparam, lparam);
	}
	}
}

bool register_window_class(HINSTANCE inst)
{
	WNDCLASSEXW wcx{ sizeof(WNDCLASSEXW) };

	wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcx.hCursor = reinterpret_cast<HCURSOR>(LoadImageW(nullptr, MAKEINTRESOURCEW(32512), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE));
	wcx.hIcon = reinterpret_cast<HICON>(LoadImageW(nullptr, MAKEINTRESOURCEW(32512), IMAGE_ICON, 0, 0, LR_SHARED | LR_DEFAULTSIZE));
	wcx.hIconSm = reinterpret_cast<HICON>(LoadImageW(nullptr, MAKEINTRESOURCEW(32512), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	wcx.hInstance = inst;
	wcx.lpszClassName = L"PickerAppContainerTest";
	wcx.lpfnWndProc = wndproc;
	wcx.style = CS_HREDRAW | CS_VREDRAW;

	return RegisterClassExW(&wcx) != 0;
}

ABI::Windows::System::IDispatcherQueueController *create_dispatcher_queue_abi()
{
	ABI::Windows::System::IDispatcherQueueController *abi_dqc{ nullptr };

	if (SUCCEEDED(CreateDispatcherQueueController(DispatcherQueueOptions{ sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE }, &abi_dqc)))
	{
		return abi_dqc;
	}

	return nullptr;
}

winrt::Windows::System::DispatcherQueueController create_dispatcher_queue()
{
	auto abi_dqc = create_dispatcher_queue_abi();
	if (abi_dqc)
	{
		return winrt::Windows::System::DispatcherQueueController{ abi_dqc, winrt::take_ownership_from_abi };
	}

	return nullptr;
}

static winrt::Windows::System::DispatcherQueueController s_dispatcher_queue_controller{ nullptr };

int APIENTRY wWinMain(_In_ HINSTANCE inst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int cmd_show)
{
	winrt::init_apartment(winrt::apartment_type::single_threaded);

	s_dispatcher_queue_controller = create_dispatcher_queue();

	if (!register_window_class(inst))
	{
		return static_cast<int>(GetLastError());
	}

	HWND window = CreateWindowExW(0, L"PickerAppContainerTest", L"Test Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, inst, nullptr);
	if (!window)
	{
		return static_cast<int>(GetLastError());
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	MSG msg{};
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return static_cast<int>(msg.wParam);
}