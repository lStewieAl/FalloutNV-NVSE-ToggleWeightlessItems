#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "nvse/nvse/GameRTTI.h"
#include "nvse/nvse/ParamInfos.h"

bool versionCheck(const NVSEInterface* nvse);

bool shouldHideItem(TESForm* form);
int getItemWeight(TESForm* form);
void injectQuestItemJMP();
void hookIsWeightless();
void(*RefreshItemListBox)(void) = (void(*)(void))0x704AF0;
String getItemName(TESForm* form);


bool hideWeightless = false;
char nameFilter[256] = {'\0'};

DEFINE_COMMAND_PLUGIN(TSW, "Toggles the visibility of weightless items in containers", 0, 0, NULL)
bool Cmd_TSW_Execute(COMMAND_ARGS) {
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

DEFINE_COMMAND_PLUGIN(Filter, "Disable visibility of specified items in containers", 0, 1, kParams_OneString)
bool Cmd_Filter_Execute(COMMAND_ARGS) {
	ExtractArgs(EXTRACT_ARGS, &nameFilter);
	RefreshItemListBox();
	
	return true;
}

DEFINE_COMMAND_PLUGIN(UnFilter, "Remove filter applied with 'filter' command", 0, 1, NULL)
bool Cmd_UnFilter_Execute(COMMAND_ARGS) {
	nameFilter[0] = '\0';
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
		injectQuestItemJMP();

		// register commands
		nvse->SetOpcodeBase(0x2000);
		nvse->RegisterCommand(&kCommandInfo_TSW);
		nvse->RegisterCommand(&kCommandInfo_Filter);
		nvse->RegisterCommand(&kCommandInfo_UnFilter);

		return true;
	}

};

void injectQuestItemJMP() {
    /* add push eax (which will contain the items base address) and shift the proceeding code down one (to occupy the NOP) */
	SafeWriteBuf(0x75E662, "\x50\x8B\xC8\x0F\xB6\x41\x04", 7);
	WriteRelJump(0x75E89F, (UInt32)hookIsWeightless);
}

__declspec(naked) void hookIsWeightless() {
	_asm {
		cmp al, 1 // if al is 1, the item should be hidden, so don't bother checking its weight
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
	if (!form) return false;
	return (hideweightless && getItemWeight(form) <= 0) || (nameFilter[0] && !getItemName(form).Includes(nameFilter));
}

int getItemWeight(TESForm* form) {
	int weight = -1;
	TESWeightForm* weightForm = DYNAMIC_CAST(form, TESForm, TESWeightForm);
	if (weightForm) {
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

String getItemName(TESForm* form) {
	TESFullName* first = DYNAMIC_CAST(form, TESForm, TESFullName);
	return first->name;
}



bool versionCheck(const NVSEInterface* nvse) {
	if (nvse->isEditor) return false;
	if (nvse->nvseVersion < NVSE_VERSION_INTEGER) {
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
		return false;
	}
	return true;
}
