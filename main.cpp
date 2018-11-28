#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "nvse/nvse/GameRTTI.h"

bool versionCheck(const NVSEInterface* nvse);

bool shouldHideItem(TESForm* form);
int getWeight(TESForm* form);
void injectShouldShowItemHooks();
void hookIsWeightless();
void(*RefreshItemListBox)(void) = (void(*)(void))0x704AF0;


static bool hideWeightless = false;

DEFINE_COMMAND_PLUGIN(TSW, "Toggles the visibility of weightless items in containers", 0, 0, NULL)
bool Cmd_TSW_Execute(COMMAND_ARGS)
{
	hideWeightless = !hideWeightless;

	if (hideWeightless) {
		Console_Print("Weightless items hidden.");
	}
	else {
		Console_Print("Weightless items shown.");
	}
	*result = hideWeightless;
	RefreshItemListBox();
	return true;
}

extern "C" {

	BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
		return TRUE;
	}

	bool NVSEPlugin_Query(const NVSEInterface *nvse, PluginInfo *info) {
		/* fill out the info structure */
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Toggle Weightless Items in Containers";
		info->version = 1;
		return versionCheck(nvse);
	}

	bool NVSEPlugin_Load(const NVSEInterface *nvse) {
		injectShouldShowItemHooks();

		// register commands
		nvse->SetOpcodeBase(0x2000);
		nvse->RegisterCommand(&kCommandInfo_TSW);

		return true;
	}

};

void injectShouldShowItemHooks() {
    /* add push eax (which will contain the items base address) and shift the proceeding code down one (to occupy the NOP) */
	SafeWriteBuf(0x75E662, "\x50\x8B\xC8\x0F\xB6\x41\x04", 7);
	WriteRelJump(0x75E89F, (UInt32)hookIsWeightless);
}

__declspec(naked) void hookIsWeightless() {
	_asm {
		cmp al, 1 // if al is already 1, the item should be hidden, so don't bother checking its weight
		je leaveFunction

		mov eax, [ebp-0x38] // eax <- item base address
		call shouldHideItem

		leaveFunction:
		mov esp,ebp
		pop ebp
		ret
	}
}

bool shouldHideItem(TESForm* form) {
	if (hideWeightless && form) return getWeight(form) <= 0;
	return false;
}

int getWeight(TESForm* form) {
	int weight = -1;
	TESWeightForm* weightForm = DYNAMIC_CAST(form, TESForm, TESWeightForm);
	if (weightForm)
	{
		weight = (int) weightForm->weight;
	}
	else {
		TESAmmo* pAmmo = DYNAMIC_CAST(form, TESForm, TESAmmo);
		if (pAmmo) {
			weight = (int) pAmmo->weight;
		}
	}
	return weight;
}



bool versionCheck(const NVSEInterface* nvse) {
	if (nvse->isEditor) return false;
	if (nvse->nvseVersion < NVSE_VERSION_INTEGER) {
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
		return false;
	}
	return true;
}
