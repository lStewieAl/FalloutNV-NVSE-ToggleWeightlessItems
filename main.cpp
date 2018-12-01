#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "nvse/nvse/GameRTTI.h"
#include "nvse/nvse/ParamInfos.h"

bool versionCheck(const NVSEInterface* nvse);

bool shouldHideItem(TESForm* form);
void injectQuestItemJMP();
void hookIsWeightless();

String getItemName(TESForm* form);

/* Credits to JazzIsParis */
void(*RefreshItemListBox)(void) = (void(*)(void))0x704AF0;
float(*GetItemWeight)(TESForm *baseItem, bool isHardcore) = (float(*)(TESForm*, bool))0x48EBC0;
ParamInfo kParams_JIP_OneOptionalString[] =
{
	{ "String", kParamType_String, 1 }
};
#define NUM_ARGS *((UInt8*)scriptData + *opcodeOffsetPtr)


bool hideWeightless = false;
char nameFilter[256] = { '\0' };

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

DEFINE_COMMAND_PLUGIN(filter, "Filter items from a container", 0, 1, kParams_JIP_OneOptionalString)
bool Cmd_filter_Execute(COMMAND_ARGS)
{
	UInt8 numArgs = NUM_ARGS;
	if (!ExtractArgs(EXTRACT_ARGS, &nameFilter) || numArgs == 0) nameFilter[0] = '\0';
	RefreshItemListBox();

	return true;
}

DEFINE_COMMAND_PLUGIN(unfilter, "Remove all filters", 0, 0, NULL)
bool Cmd_unfilter_Execute(COMMAND_ARGS)
{
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
		info->name = "Inventory Filter";
		info->version = 1;
		return versionCheck(nvse);
	}

	bool NVSEPlugin_Load(const NVSEInterface *nvse) {
		if (!(nvse->isEditor)) injectQuestItemJMP();

		// register commands
		nvse->SetOpcodeBase(0x2130);
		nvse->RegisterCommand(&kCommandInfo_TSW);
		nvse->RegisterCommand(&kCommandInfo_filter);
		nvse->RegisterCommand(&kCommandInfo_unfilter);

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

		mov eax, [ebp - 0x38] // eax <- item base address
		call shouldHideItem

		leaveFunction :
		mov esp, ebp
			pop ebp
			ret
	}
}

bool shouldHideItem(TESForm* form) {
	if (!form) return false;

	if (hideWeightless && GetItemWeight(form, false) <= 0) {
		return true;
	}
	if (nameFilter[0] && !getItemName(form).Includes(nameFilter)) {
		return true;
	}
	return false;
}

String getItemName(TESForm* form) {
	TESFullName* first = DYNAMIC_CAST(form, TESForm, TESFullName);
	return first->name;
}



bool versionCheck(const NVSEInterface* nvse) {
	if (nvse->nvseVersion < NVSE_VERSION_INTEGER) {
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
		return false;
	}
	return true;
}
