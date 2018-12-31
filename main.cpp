#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "nvse/nvse/GameRTTI.h"
#include "nvse/nvse/ParamInfos.h"

bool versionCheck(const NVSEInterface* nvse);

bool __fastcall shouldHideItem(TESForm* form);
void injectHooks();
void hookCrafting();
void hookContainer();
float getValuePerWeight(TESForm* form);

String getItemName(TESForm* form);

/* Credits to JazzIsParis */
void(*RefreshItemListBox)(void) = (void(*)(void))0x704AF0;
float(*GetItemWeight)(TESForm *baseItem, bool isHardcore) = (float(*)(TESForm*, bool))0x48EBC0;
ParamInfo kParams_JIP_OneOptionalString[] =
{
	{ "String", kParamType_String, 1 }
};
#define NUM_ARGS *((UInt8*)scriptData + *opcodeOffsetPtr)

UInt32 GetItemValue(TESForm* form)
{
	if (form->typeID == kFormType_AlchemyItem) return ((AlchemyItem*)form)->value;
	TESValueForm *valForm = DYNAMIC_CAST(form, TESForm, TESValueForm);
	return valForm ? valForm->value : 0;
}


bool hideWeightless = false;
char nameFilter[256] = { '\0' };
int hideItemValue = 0;

DEFINE_COMMAND_PLUGIN(TSW, "Toggles the visibility of weightless items in containers", 0, 0, NULL)
bool Cmd_TSW_Execute(COMMAND_ARGS)
{
	hideWeightless = !hideWeightless;
	if (IsConsoleMode) Console_Print("Weightless items: %s", hideWeightless ? "hidden.":"shown.");

	*result = hideWeightless;
	RefreshItemListBox();
	return true;
}

DEFINE_COMMAND_PLUGIN(filter, "Filter items from a container", 0, 1, kParams_JIP_OneOptionalString)
bool Cmd_filter_Execute(COMMAND_ARGS)
{
	UInt8 numArgs = NUM_ARGS;
	if (!ExtractArgs(EXTRACT_ARGS, &nameFilter) || numArgs == 0) nameFilter[0] = '\0';

	return true;
}

DEFINE_COMMAND_PLUGIN(filterval, "Filter based on value/weight", 0, 1, kParams_OneOptionalInt)
bool Cmd_filterval_Execute(COMMAND_ARGS)
{
	UInt8 numArgs = NUM_ARGS;
	if (!ExtractArgs(EXTRACT_ARGS, &hideItemValue) || numArgs == 0) hideItemValue = 0;
	return true;
}

DEFINE_COMMAND_PLUGIN(unfilter, "Remove all filters", 0, 0, NULL)
bool Cmd_unfilter_Execute(COMMAND_ARGS)
{
	nameFilter[0] = '\0';
	hideWeightless = false;
	hideItemValue = 0;
	return true;
}

DEFINE_COMMAND_PLUGIN(isFiltered, "Check whether a filter is active", 0, 0, NULL)
bool Cmd_isFiltered_Execute(COMMAND_ARGS)
{
	bool isFilter = nameFilter[0] != '\0' || hideWeightless;
	if (IsConsoleMode) Console_Print("IsFiltered: %s", isFilter ? "true":"false");
	*result = isFilter;
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
		if (!(nvse->isEditor)) injectHooks();

		// register commands
		nvse->SetOpcodeBase(0x2130);
		nvse->RegisterCommand(&kCommandInfo_TSW);
		nvse->RegisterCommand(&kCommandInfo_filter);
		nvse->RegisterCommand(&kCommandInfo_unfilter);
		nvse->RegisterCommand(&kCommandInfo_isFiltered);
		nvse->RegisterCommand(&kCommandInfo_filterval);

		return true;
	}

};

void injectHooks() {

	/* crafting/recipe hook */
	WriteRelJump(0x728D99, (UInt32)hookCrafting);

	/* general inventory hook (container, pipboy, barter) */
	WriteRelJump(0x730C8F, (UInt32)hookContainer);
	SafeWrite8(0x730C94, 0x90);

}

_declspec(naked) void hookContainer() {
	static const UInt32 retnAddr = 0x730C95;
	_asm {
		pop ecx
		test al, al
		jnz skipCheck

		mov ecx, [ecx+8]
		call shouldHideItem
		
	skipCheck :
		movzx eax, al
		jmp retnAddr
	}
}


__declspec(naked) void hookCrafting() {
	_asm {
		test al, al // if al is already 1, the item should be hidden, so don't bother checking whether it should be hidden
		jnz leaveFunction
		mov ecx, [ebp + 8]
		call shouldHideItem

	leaveFunction:	
		pop esi
		pop ebp
		ret
	}
}

bool __fastcall shouldHideItem(TESForm* form) {
	if (!form) return false;

	if (hideItemValue && (getValuePerWeight(form) < hideItemValue)) return true;

	if (hideWeightless && GetItemWeight(form, false) <= 0) {
		return true;
	}
	if (nameFilter[0] && !getItemName(form).Includes(nameFilter)) {
		return true;
	}
	return false;
}

float getValuePerWeight(TESForm* form) {
	float weight = GetItemWeight(form, false);
	return weight ? (float) GetItemValue(form) / weight : INT_MAX;
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
