#pragma once
#include "pch.h"
#include "config.h"
#include "res/resources.h"
#include "LxRunOfflineShellExt_i.h"

using namespace ATL;

class ATL_NO_VTABLE __declspec(uuid(HANDLER_CLSID_STR)) CContextMenuHandler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CContextMenuHandler, &CLSID_ContextMenuHandler>,
	public IShellExtInit,
	public IContextMenu {

	BEGIN_COM_MAP(CContextMenuHandler)
		COM_INTERFACE_ENTRY(IShellExtInit)
		COM_INTERFACE_ENTRY(IContextMenu)
	END_COM_MAP()
	DECLARE_NOT_AGGREGATABLE(CContextMenuHandler)
	DECLARE_REGISTRY_RESOURCEID(IDR_CONTEXTMENUHANDLER)

private:
	std::wstring path;
	std::vector<std::wstring> distros;
	HMENU hsm = nullptr;

public:
	void FinalRelease() const;

	IFACEMETHODIMP Initialize(
		PCIDLIST_ABSOLUTE pidlFolder,
		IDataObject *pdtobj,
		HKEY hkeyProgID
	) override;

	IFACEMETHODIMP QueryContextMenu(
		HMENU hmenu,
		UINT indexMenu,
		UINT idCmdFirst,
		UINT idCmdLast,
		UINT uFlags
	) override;

	IFACEMETHODIMP GetCommandString(
		UINT_PTR idCmd,
		UINT uType,
		UINT *pReserved,
		LPSTR pszName,
		UINT cchMax
	) override;

	IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO *pici) override;
};

OBJECT_ENTRY_AUTO(__uuidof(ContextMenuHandler), CContextMenuHandler)
