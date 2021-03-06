#include "./receiver.hpp"
#include "./string.hpp"
#include "./array.hpp"

#include <angelscript.h>

#include "../system/input.hpp"
#include "../system/audio.hpp"
#include "../system/music.hpp"
#include "../system/kernel.hpp"

#include "../utility/constants.hpp"
#include "../utility/logger.hpp"
#include "../utility/vfs.hpp"

#include "../overlay/draw_headsup.hpp"
#include "../overlay/draw_title_view.hpp"

#include "../menu/stack_gui.hpp"
#include "../menu/dialogue_gui.hpp"
#include "../menu/inventory_gui.hpp"

#include "../field/camera.hpp"
#include "../component/kontext.hpp"
#include "../actor/naomi.hpp"

static const byte_t kBootFile[]	 = "boot";
static const byte_t kDeathDecl[] = "void death(arch_t type)";
static const byte_t kInvenDecl[] = "void inventory(arch_t type, arch_t cursor)";

static constexpr uint_t kMaxCalls = 128;

receiver_t::receiver_t() :
	bitmask(0),
	timer(0.0f),
	calls(0),
	engine(nullptr),
	state(nullptr),
	current(nullptr),
	boot(nullptr),
	events()
{
	
}

receiver_t::~receiver_t() {
	this->reset();
	if (boot != nullptr) {
		boot->Release();
		boot = nullptr;
	}
	if (state != nullptr) {
		state->Release();
		state = nullptr;
	}
	if (engine != nullptr) {
		engine->ShutDownAndRelease();
		engine = nullptr;
	}
}

bool receiver_t::init(input_t& input, audio_t& audio, music_t& music, kernel_t& kernel, stack_gui_t& stack_gui, dialogue_gui_t& dialogue_gui, draw_title_view_t& title_view, draw_headsup_t& headsup, camera_t& camera, naomi_state_t& naomi_state, kontext_t& kontext) {
	if (engine != nullptr) {
		synao_log("Scripting engine already exists!\n");
		return false;
	}
	if (state != nullptr) {
		synao_log("Scripting state already exists!\n");
		return false;
	}
	if (boot != nullptr) {
		synao_log("Scripting boot function already exists!\n");
		return false;
	}
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if (engine == nullptr) {
		synao_log("Scripting engine creation failed!\n");
		return false;
	}
	this->generate_properties();
	this->generate_functions(
		input, audio, music,
		kernel, stack_gui, dialogue_gui,
		title_view, headsup, camera,
		naomi_state, kontext
	);
	state = engine->CreateContext();
	if (state == nullptr) {
		synao_log("Scripting state creation failed!\n");
		return false;
	}
	if (state->SetLineCallback(asFUNCTION(calls_callback), &calls, asCALL_CDECL) < 0) {
		synao_log("Creating line callback for script context failed!\n");
		return false;
	}
	if (!this->load(kBootFile, rec_loading_t::Global)) {
		synao_log("Global module loading failed!\n");
		return false;
	}
	asIScriptModule* boot_module = engine->GetModuleByIndex(0);
	boot = boot_module->GetFunctionByIndex(0);
	if (boot == nullptr) {
		synao_log("Couldn't find any functions in boot module!\n");
		return false;
	}
	boot->AddRef();
	synao_log("Receiver subsystem initialized.\n");
	return true;
}

void receiver_t::reset() {
	if (bitmask[rec_bits_t::Running]) {
		state->Abort();
		state->Unprepare();
	}
	bitmask[rec_bits_t::Running] = false;
	bitmask[rec_bits_t::Waiting] = false;
	bitmask[rec_bits_t::Stalled] = false;
	timer = 0.0f;
	calls = 0;
	this->discard_all_events();
}

void receiver_t::handle(const input_t& input, kernel_t& kernel, const stack_gui_t& stack_gui, dialogue_gui_t& dialogue_gui, const inventory_gui_t& inventory_gui, draw_headsup_t& headsup) {
	if (bitmask[rec_bits_t::Running]) {
		if (!headsup.is_fade_moving() and !dialogue_gui.get_flag(dialogue_flag_t::Question)) {
			if (bitmask[rec_bits_t::Stalled]) {
				if (!dialogue_gui.get_flag(dialogue_flag_t::Writing) and input.pressed[btn_t::Yes]) {
					bitmask[rec_bits_t::Running] = true;
					bitmask[rec_bits_t::Waiting] = false;
					bitmask[rec_bits_t::Stalled] = false;
					timer = 0.0f;
					calls = 0;
				}
			} else if (!bitmask[rec_bits_t::Waiting]) {
				sint_t r = state->Execute();
				switch (r) {
				case asEXECUTION_SUSPENDED: {
					break;
				}
				case asEXECUTION_FINISHED:
				case asEXECUTION_ABORTED: {
					this->close_dependencies(
						kernel,
						stack_gui,
						inventory_gui,
						dialogue_gui
					);
					break;
				}
				case asEXECUTION_EXCEPTION: {
					bitmask.reset();
					timer = 0.0f;
					calls = 0;
					synao_log(
						"Running script threw an exception!\n%s\n%s at line %d!\n",
						state->GetExceptionString(),
						state->GetExceptionFunction()->GetName(),
						state->GetExceptionLineNumber()
					);
					state->Abort();
					state->Unprepare();
					break;
				}
				default: {
					break;
				}
				}
			}
		}
	}
}

void receiver_t::update(real64_t delta) {
	if (bitmask[rec_bits_t::Waiting]) {
		timer -= static_cast<real_t>(delta);
		if (timer <= 0.0f) {
			bitmask[rec_bits_t::Running] = true;
			bitmask[rec_bits_t::Waiting] = false;
			bitmask[rec_bits_t::Stalled] = false;
			timer = 0.0f;
			calls = 0;
		}
	}
}

bool receiver_t::running() const {
	return bitmask[rec_bits_t::Running];
}

bool receiver_t::load(const kernel_t& kernel) {
	return this->load(
		kernel.get_field(),
		rec_loading_t::None
	);
}

bool receiver_t::load(const std::string& name) {
	return this->load(name, rec_loading_t::None);
}

bool receiver_t::load(const std::string& name, rec_loading_t flags) {
	if (engine == nullptr) {
		synao_log("Scripting engine doesn't exist!\nCouldn't load module!\n");
		return false;
	}
	asIScriptModule* module = engine->GetModule(name.c_str(), asGM_CREATE_IF_NOT_EXISTS);
	if (module == nullptr) {
		synao_log("Couldn't allocate script module during loading process!\n");
		return false;
	}
	const std::string buffer = vfs::string_buffer(vfs::event_path(name, flags));
	arch_t length = buffer.length();
	if (module->AddScriptSection(name.c_str(), buffer.c_str(), length) != 0) {
		current = nullptr;
		synao_log("Adding script section %s failed!\n", name.c_str());
		return false;
	}
	if (module->Build() != 0) {
		current = nullptr;
		synao_log("Building module %s failed!\n", name.c_str());
		return false;
	}
	this->link_imported_functions(module);
	if (flags == rec_loading_t::None) {
		current = module;
	}
	return true;
}

void receiver_t::run_function(const kernel_t& kernel) {
	if (!bitmask[rec_bits_t::Running]) {
		asIScriptFunction* kernel_function = kernel.get_function();
		if (kernel.has(kernel_state_t::Zero)) {
			this->execute_function(boot);
		} else if (kernel_function != nullptr) {
			this->execute_function(kernel_function);
		} else {
			const std::string field = kernel.get_field();
			asIScriptFunction* main_function = this->find_from_index(field, 0);
			this->execute_function(main_function);
		}
	}
}

void receiver_t::run_event(sint_t id) {
	if (!bitmask[rec_bits_t::Running] and id != 0) {
		auto it = events.find(id);
		if (it != events.end()) {
			this->execute_function(it->second);
		}
	}
}

void receiver_t::run_inventory(arch_t type, arch_t index) {
	if (!bitmask[rec_bits_t::Running]) {
		const std::vector<arch_t> args = { type, index };
		asIScriptFunction* function = this->find_from_declaration(kInvenDecl);
		this->execute_function(function, args);
	}
}

void receiver_t::run_death(arch_t type) {
	if (!bitmask[rec_bits_t::Running]) {
		const std::vector<arch_t> args = { type };
		asIScriptFunction* function = this->find_from_declaration(kDeathDecl);
		this->execute_function(function, args);
	}
}

void receiver_t::push_from_symbol(sint_t id, const std::string& module_name, const std::string& symbol) {
	auto it = events.find(id);
	if (it != events.end()) {
		if (it->second != nullptr) {
			it->second->Release();
			it->second = nullptr;
		}
	}
	asIScriptFunction* function = this->find_from_symbol(module_name, symbol);
	if (function != nullptr) {
		function->AddRef();
		events[id] = function;
	}
}

void receiver_t::push_from_function(sint_t id, asIScriptFunction* function) {
	auto it = events.find(id);
	if (it != events.end()) {
		if (it->second != nullptr) {
			it->second->Release();
			it->second = nullptr;
		}
	}
	if (function != nullptr) {
		events[id] = function;
	}
}

void receiver_t::suspend() {
	if (bitmask[rec_bits_t::Running]) {
		bitmask[rec_bits_t::Running] = true;
		bitmask[rec_bits_t::Waiting] = false;
		bitmask[rec_bits_t::Stalled] = false;
		timer = 0.0f;
		calls = 0;
		sint_t r = state->Suspend();
		assert(r >= 0);
	}
}

void receiver_t::print_message(const std::string& message) {
	synao_log("%s\n", message.c_str());
}

void receiver_t::error_callback(const asSMessageInfo* msg, optr_t) {
	const byte_t* type = nullptr;
	switch (msg->type) {
	case asMSGTYPE_ERROR:
		type = "ERROR";
		break;
	case asMSGTYPE_WARNING:
		type = "WARN";
		break;
	case asMSGTYPE_INFORMATION:
		type = "INFO";
		break;
	default:
		type = "OKAY";
		break;
	}
	synao_log(
		"%s (%d, %d) : %s : %s\n",
		msg->section,
		msg->row, msg->col,
		type, msg->message
	);
}

void receiver_t::calls_callback(asIScriptContext* ctx, uint_t* calls) {
	assert(calls != nullptr);
	(*calls)++;
	if (*calls >= kMaxCalls) {
		*calls = 0;
		ctx->Suspend();
	}
}

asIScriptFunction* receiver_t::find_from_index(const std::string& module_name, arch_t index) const {
	asIScriptModule* module = engine->GetModule(module_name.c_str(), asGM_ONLY_IF_EXISTS);
	if (module != nullptr) {
		asIScriptFunction* function = module->GetFunctionByIndex(static_cast<asUINT>(index));
		if (function != nullptr) {
			return function;
		}
	}
	return nullptr;
}

asIScriptFunction* receiver_t::find_from_symbol(const std::string& module_name, const std::string& symbol) const {
	const std::string declaration = "void " + symbol + "()";
	asIScriptModule* module = engine->GetModule(module_name.c_str(), asGM_ONLY_IF_EXISTS);
	if (module != nullptr) {
		asIScriptFunction* function = module->GetFunctionByDecl(declaration.c_str());
		if (function != nullptr) {
			return function;
		}
	}
	return engine->GetModuleByIndex(0)->GetFunctionByDecl(declaration.c_str());
}

asIScriptFunction* receiver_t::find_from_declaration(const std::string& module_name, const std::string& declaration) const {
	asIScriptModule* module = engine->GetModule(module_name.c_str(), asGM_ONLY_IF_EXISTS);
	if (module != nullptr) {
		return module->GetFunctionByDecl(declaration.c_str());
	}
	return nullptr;
}

asIScriptFunction* receiver_t::find_from_declaration(const std::string& declaration) const {
	asIScriptModule* module = engine->GetModuleByIndex(0);
	if (module != nullptr) {
		return module->GetFunctionByDecl(declaration.c_str());
	}
	return nullptr;
}

bool receiver_t::unlinked(asIScriptModule* boot_module, asIScriptModule* src_module) const {
	asUINT count = boot_module->GetImportedFunctionCount();
	const byte_t* src_name = src_module->GetName();
	for (asUINT it = 0; it < count; ++it) {
		const byte_t* imp_name = boot_module->GetImportedFunctionSourceModule(it);
		if (std::strcmp(src_name, imp_name) == 0) {
			return false;
		}
	}
	return true;
}

void receiver_t::execute_function(asIScriptFunction* function) {
	if (function != nullptr and state->Prepare(function) >= 0) {
		bitmask[rec_bits_t::Running] = true;
		bitmask[rec_bits_t::Waiting] = false;
		bitmask[rec_bits_t::Stalled] = false;
		timer = 0.0f;
		calls = 0;
	} else {
		synao_log("Couldn't execute function!\n");
	}
}

void receiver_t::execute_function(asIScriptFunction* function, std::vector<arch_t> args) {
	if (function != nullptr and state->Prepare(function) >= 0) {
		bitmask[rec_bits_t::Running] = true;
		bitmask[rec_bits_t::Waiting] = false;
		bitmask[rec_bits_t::Stalled] = false;
		timer = 0.0f;
		calls = 0;
		for (arch_t it = 0; it < args.size(); ++it) {
			if constexpr (sizeof(arch_t) == 8) {
				if (state->SetArgQWord(static_cast<uint_t>(it), args[it]) < 0) {
					synao_log("Couldn't set argument %d!\n", static_cast<uint_t>(it));
				}
			} else {
				if (state->SetArgDWord(static_cast<uint_t>(it), args[it]) < 0) {
					synao_log("Couldn't set argument %d!\n", static_cast<uint_t>(it));
				}
			}
		}
	} else {
		synao_log("Couldn't execute function!\n");
	}
}

void receiver_t::close_dependencies(kernel_t& kernel, const stack_gui_t& stack_gui, const inventory_gui_t& inventory_gui, dialogue_gui_t& dialogue_gui) {
	bitmask.reset();
	timer = 0.0f;
	calls = 0;
	state->Unprepare();
	if (stack_gui.empty() and !inventory_gui.open()) {
		kernel.unlock();
	}
	stack_gui.invalidate();
	dialogue_gui.close_textbox();
}

void receiver_t::discard_all_events() {
	// asIScriptModule* boot_module = engine->GetModuleByIndex(0);
	if (current != nullptr and current->GetImportedFunctionCount() == 0) {
		for (uint_t it = 1; it < engine->GetModuleCount(); ++it) {
			asIScriptModule* module = engine->GetModuleByIndex(it);
			if (module != nullptr /*and this->unlinked(boot_module, module)*/) {
				engine->DiscardModule(module->GetName());
			}
		}
	}
	current = nullptr;
	for (auto&& event : events) {
		if (event.second != nullptr) {
			event.second->Release();
			event.second = nullptr;
		}
	}
	events.clear();
}

void receiver_t::link_imported_functions(asIScriptModule* module) {
	uint_t total = module->GetImportedFunctionCount();
	for (uint_t it = 0; it < total; ++it) {
		const byte_t* name = module->GetImportedFunctionSourceModule(it);
		asIScriptModule* query = engine->GetModule(name, asGM_CREATE_IF_NOT_EXISTS);
		if (query == nullptr) {
			synao_log("Couldn't allocate script module during linking process!\n");
			break;
		}
		if (query->GetFunctionCount() == 0 and !this->load(name, rec_loading_t::Import)) {
			synao_log("Couldn't load script module during linking process!\n");
			break;
		}
		const byte_t* declaration = module->GetImportedFunctionDeclaration(it);
		asIScriptFunction* function = module->GetFunctionByDecl(declaration);
		if (function == nullptr or module->BindImportedFunction(it, function) < 0) {
			synao_log("Linking for declaration %s failed!\n", declaration);
		}
	}
}

void receiver_t::set_stalled_period() {
	bitmask[rec_bits_t::Running] = true;
	bitmask[rec_bits_t::Waiting] = false;
	bitmask[rec_bits_t::Stalled] = true;
	timer = 0.0f;
	calls = 0;
	state->Suspend();
}

void receiver_t::set_waiting_period(real_t seconds) {
	bitmask[rec_bits_t::Running] = true;
	bitmask[rec_bits_t::Waiting] = true;
	bitmask[rec_bits_t::Stalled] = false;
	timer = seconds;
	state->Suspend();
}

#define WRAP_FN(name) 											asFUNCTION(name)
#define WRAP_FN_PR(name, Parameters, ReturnType) 				asFUNCTIONPR(name, Parameters, ReturnType)
#define WRAP_MFN(ClassType, name) 								asMETHOD(ClassType, name)
#define WRAP_MFN_PR(ClassType, name, Parameters, ReturnType) 	asMETHODPR(ClassType, name, Parameters, ReturnType)

void receiver_t::generate_properties() {
	sint_t r = 0;
	// Set Enum Scope
	r = engine->SetEngineProperty(asEEngineProp::asEP_REQUIRE_ENUM_SCOPE, 1);
	assert(r >= 0);
	// Set Error Callback
	r = engine->SetMessageCallback(WRAP_FN(receiver_t::error_callback), nullptr, asCALL_CDECL);
	assert(r >= 0);
	// Set Typedefs
	r = engine->RegisterTypedef("sint8_t", "int8");
	assert(r >= 0);
	r = engine->RegisterTypedef("sint16_t", "int16");
	assert(r >= 0);
	r = engine->RegisterTypedef("sint32_t", "int");
	assert(r >= 0);
	r = engine->RegisterTypedef("sint64_t", "int64");
	assert(r >= 0);
	r = engine->RegisterTypedef("real32_t", "float");
	assert(r >= 0);
	r = engine->RegisterTypedef("real64_t", "double");
	assert(r >= 0);
	if constexpr (sizeof(arch_t) == 8) {
		r = engine->RegisterTypedef("arch_t", "uint64");
	} else {
		r = engine->RegisterTypedef("arch_t", "uint");
	}
	// Register String Type
	RegisterStdString(engine);
	// Register Array Type
	RegisterScriptArray(engine, true);
	// Set Direction Enum
	r = engine->RegisterEnum("dir_t");
	assert(r >= 0);
	// Direction Indices
	r = engine->RegisterEnumValue("dir_t", "Right", 0);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "Left", 1);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "RightUp", 2);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "LeftUp", 3);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "RightDown", 4);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "LeftDown", 5);
	assert(r >= 0);
	r = engine->RegisterEnumValue("dir_t", "None", 6);
	assert(r >= 0);
	// Input Enum
	r = engine->RegisterEnum("input_t");
	assert(r >= 0);
	r = engine->RegisterEnumValue("input_t", "Yes", 0);
	assert(r >= 0);
	r = engine->RegisterEnumValue("input_t", "No", 1);
	// Set Font Enum
	r = engine->RegisterEnum("font_t");
	assert(r >= 0);
	// Font Names
	r = engine->RegisterEnumValue("font_t", "Zero", 0);
	assert(r >= 0);
	r = engine->RegisterEnumValue("font_t", "One", 1);
	assert(r >= 0);
	r = engine->RegisterEnumValue("font_t", "Two", 2);
	assert(r >= 0);
	r = engine->RegisterEnumValue("font_t", "Three", 3);
	assert(r >= 0);
	// Set Funcdef
	r = engine->SetDefaultNamespace("std");
	assert(r >= 0);
	r = engine->RegisterFuncdef("void event()");
	assert(r >= 0);
	r = engine->SetDefaultNamespace("");
	assert(r >= 0);
}

void receiver_t::generate_functions(input_t& input, audio_t& audio, music_t& music, kernel_t& kernel, stack_gui_t& stack_gui, dialogue_gui_t& dialogue_gui, draw_title_view_t& title_view, draw_headsup_t& headsup, camera_t& camera, naomi_state_t& naomi_state, kontext_t& kontext) {
	sint_t r = 0;
	// Set Namespace
	r = engine->SetDefaultNamespace("sys");
	assert(r >= 0);
	// Print Statement (Debug Only)
	r = engine->RegisterGlobalFunction("void print(const std::string &in statement)", WRAP_FN(receiver_t::print_message), asCALL_CDECL);
	assert(r >= 0);
	// Stall For Script
	r = engine->RegisterGlobalFunction("void wait()", WRAP_MFN(receiver_t, set_stalled_period), asCALL_THISCALL_ASGLOBAL, this);
	assert(r >= 0);
	// Wait For Script
	r = engine->RegisterGlobalFunction("void wait(real32_t seconds)", WRAP_MFN(receiver_t, set_waiting_period), asCALL_THISCALL_ASGLOBAL, this);
	assert(r >= 0);
	// Suspend Current Script
	r = engine->RegisterGlobalFunction("void suspend()", WRAP_MFN(receiver_t, suspend), asCALL_THISCALL_ASGLOBAL, this);
	assert(r >= 0);
	// Getting Flags
	r = engine->RegisterGlobalFunction("bool get_flag(arch_t index)", WRAP_MFN(kernel_t, get_flag), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Setting Flags
	r = engine->RegisterGlobalFunction("void set_flag(arch_t index, bool value)", WRAP_MFN(kernel_t, set_flag), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Setting Items
	r = engine->RegisterGlobalFunction("void set_item(sint32_t type, sint32_t count, sint32_t limit, sint32_t optor)", WRAP_MFN(kernel_t, set_item), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Setting Item Limit
	r = engine->RegisterGlobalFunction("void set_item_limit(sint32_t type, sint32_t limit)", WRAP_MFN(kernel_t, set_item_limit), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Setting Item Optor
	r = engine->RegisterGlobalFunction("void set_item_optor(sint32_t type, sint32_t optor)", WRAP_MFN(kernel_t, set_item_optor), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Adding Items
	r = engine->RegisterGlobalFunction("void add_item(sint32_t type, sint32_t count, sint32_t limit)", WRAP_MFN(kernel_t, add_item), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Sutracting Items
	r = engine->RegisterGlobalFunction("void sub_item(sint32_t type, sint32_t count)", WRAP_MFN(kernel_t, sub_item), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Ridding Items
	r = engine->RegisterGlobalFunction("void rid_item(sint32_t type)", WRAP_MFN(kernel_t, rid_item), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Polling Items
	r = engine->RegisterGlobalFunction("sint32_t get_item_count(sint32_t type)", WRAP_MFN(kernel_t, get_item_count), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Set ItmPtr Index
	r = engine->RegisterGlobalFunction("void set_item_ptr_index(arch_t index)", WRAP_MFN_PR(kernel_t, set_item_ptr_index, (arch_t), void), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Set ItmPtr Index
	r = engine->RegisterGlobalFunction("void set_item_ptr_index()", WRAP_MFN_PR(kernel_t, set_item_ptr_index, (void), void), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Get ItmPtr Index
	r = engine->RegisterGlobalFunction("arch_t get_item_ptr_index()", WRAP_MFN(kernel_t, get_item_ptr_index), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Get Max Items
	r = engine->RegisterGlobalFunction("arch_t get_max_items()", WRAP_MFN(kernel_t, get_max_items), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Lock Keys
	r = engine->RegisterGlobalFunction("void lock()", WRAP_MFN(kernel_t, lock), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Freeze Keys
	r = engine->RegisterGlobalFunction("void freeze()", WRAP_MFN(kernel_t, freeze), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Unlock Keys
	r = engine->RegisterGlobalFunction("void unlock()", WRAP_MFN(kernel_t, unlock), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Restart Game
	r = engine->RegisterGlobalFunction("void boot()", WRAP_MFN(kernel_t, boot), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Quit Game
	r = engine->RegisterGlobalFunction("void quit()", WRAP_MFN(kernel_t, quit), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Set Room Default Function
	r = engine->RegisterGlobalFunction("void set_field(const std::string &in field, sint32_t id)", WRAP_MFN_PR(kernel_t, buffer_field, (const std::string&, sint_t), void), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Set Room Special Function
	r = engine->RegisterGlobalFunction("void set_field(const std::string &in field, sint32_t id, std::event@ event)", WRAP_MFN_PR(kernel_t, buffer_field, (const std::string&, sint_t, asIScriptFunction*), void), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Load Progress
	r = engine->RegisterGlobalFunction("void load_progress()", WRAP_MFN(kernel_t, load_progress), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Save Progress
	r = engine->RegisterGlobalFunction("void save_progress()", WRAP_MFN(kernel_t, save_progress), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Load CheckPoint
	r = engine->RegisterGlobalFunction("void load_checkpoint()", WRAP_MFN(kernel_t, load_checkpoint), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Save CheckPoint
	r = engine->RegisterGlobalFunction("void save_checkpoint()", WRAP_MFN(kernel_t, save_checkpoint), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Set FileIndex
	r = engine->RegisterGlobalFunction("void set_file_index(arch_t index)", WRAP_MFN(kernel_t, set_file_index), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Get FileIndex
	r = engine->RegisterGlobalFunction("arch_t get_file_index()", WRAP_MFN(kernel_t, get_file_index), asCALL_THISCALL_ASGLOBAL, &kernel);
	assert(r >= 0);
	// Random Int
	r = engine->RegisterGlobalFunction("sint32_t get_rand(sint32_t lowest, sint32_t highest)", WRAP_FN_PR(rng::next, (sint_t, sint_t), sint_t), asCALL_CDECL);
	assert(r >= 0);
	// Random Float
	r = engine->RegisterGlobalFunction("real32_t get_rand(real32_t lowest, real32_t highest)", WRAP_FN_PR(rng::next, (real_t, real_t), real_t), asCALL_CDECL);
	assert(r >= 0);
	// Get Input Press
	r = engine->RegisterGlobalFunction("bool get_key_press(arch_t action)", WRAP_MFN(std::bitset<btn_t::Total>, test), asCALL_THISCALL_ASGLOBAL, &input.pressed);
	assert(r >= 0);
	// Get Input Held
	r = engine->RegisterGlobalFunction("bool get_key_held(arch_t action)", WRAP_MFN(std::bitset<btn_t::Total>, test), asCALL_THISCALL_ASGLOBAL, &input.holding);
	assert(r >= 0);
	// Get Locale String
	r = engine->RegisterGlobalFunction("std::string locale(const std::string &in key, arch_t index)", WRAP_FN_PR(vfs::i18n_find, (const std::string&, arch_t), std::string), asCALL_CDECL);
	assert(r >= 0);
	// Get Locale String
	r = engine->RegisterGlobalFunction("std::string locale(const std::string &in key, arch_t first, arch_t last)", WRAP_FN_PR(vfs::i18n_find, (const std::string&, arch_t, arch_t), std::string), asCALL_CDECL);
	assert(r >= 0);
	// Get Locale Size
	r = engine->RegisterGlobalFunction("arch_t locale(const std::string &in key)", WRAP_FN_PR(vfs::i18n_size, (const std::string&), arch_t), asCALL_CDECL);
	assert(r >= 0);
	// Push Menu
	r = engine->RegisterGlobalFunction("void push_widget(arch_t type, arch_t flags)", WRAP_MFN(stack_gui_t, push), asCALL_THISCALL_ASGLOBAL, &stack_gui);
	assert(r >= 0);
	// Pop Menu
	r = engine->RegisterGlobalFunction("void pop_widget()", WRAP_MFN(stack_gui_t, pop), asCALL_THISCALL_ASGLOBAL, &stack_gui);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("msg");
	assert(r >= 0);
	// Fade In
	r = engine->RegisterGlobalFunction("void fade_in()", WRAP_MFN(draw_headsup_t, fade_in), asCALL_THISCALL_ASGLOBAL, &headsup);
	assert(r >= 0);
	// Fade Out
	r = engine->RegisterGlobalFunction("void fade_out()", WRAP_MFN(draw_headsup_t, fade_out), asCALL_THISCALL_ASGLOBAL, &headsup);
	assert(r >= 0);
	// Set Room Text
	r = engine->RegisterGlobalFunction("void set_field_text(const std::string &in text)", WRAP_MFN_PR(draw_title_view_t, set_head, (const std::string&), void), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Set Room Text
	r = engine->RegisterGlobalFunction("void set_field_text()", WRAP_MFN_PR(draw_title_view_t, set_head, (void), void), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Set Facebox
	r = engine->RegisterGlobalFunction("void set_face(arch_t index, arch_t type)", WRAP_MFN_PR(dialogue_gui_t, set_face, (arch_t, direction_t), void), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Set No Facebox
	r = engine->RegisterGlobalFunction("void set_face()", WRAP_MFN_PR(dialogue_gui_t, set_face, (void), void), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Set Text Delay
	r = engine->RegisterGlobalFunction("void set_delay(real32_t delay)", WRAP_MFN_PR(dialogue_gui_t, set_delay, (real_t), void), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Set No Text Delay
	r = engine->RegisterGlobalFunction("void set_delay()", WRAP_MFN_PR(dialogue_gui_t, set_delay, (void), void), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Push Titlecard
	r = engine->RegisterGlobalFunction("void push_card(const std::string &in text, arch_t font)", WRAP_MFN(draw_title_view_t, push), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Clear Titlecard
	r = engine->RegisterGlobalFunction("void clear_cards()", WRAP_MFN(draw_title_view_t, clear), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Set Titlecard Position
	r = engine->RegisterGlobalFunction("void set_card_position(arch_t index, real32_t x, real32_t y)", WRAP_MFN_PR(draw_title_view_t, set_position, (arch_t, real_t, real_t), void), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Set Titlecard Centered
	r = engine->RegisterGlobalFunction("void set_card_centered(arch_t index, bool x, bool y)", WRAP_MFN(draw_title_view_t, set_centered), asCALL_THISCALL_ASGLOBAL, &title_view);
	assert(r >= 0);
	// Open Textbox Top
	r = engine->RegisterGlobalFunction("void top_box()", WRAP_MFN(dialogue_gui_t, open_textbox_high), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Open Textbox Bottom
	r = engine->RegisterGlobalFunction("void low_box()", WRAP_MFN(dialogue_gui_t, open_textbox_low), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Set Textbox Text
	r = engine->RegisterGlobalFunction("void say(const std::string &in words)", WRAP_MFN(dialogue_gui_t, write_textbox), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Clear Textbox
	r = engine->RegisterGlobalFunction("void clear()", WRAP_MFN(dialogue_gui_t, clear_textbox), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Close Textbox
	r = engine->RegisterGlobalFunction("void close()", WRAP_MFN(dialogue_gui_t, close_textbox), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Ask Question
	r = engine->RegisterGlobalFunction("void ask(const std::array<std::string> &in question)", WRAP_MFN_PR(dialogue_gui_t, ask_question, (const CScriptArray*), void), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);
	// Get Answer
	r = engine->RegisterGlobalFunction("arch_t get_answer()", WRAP_MFN(dialogue_gui_t, get_answer), asCALL_THISCALL_ASGLOBAL, &dialogue_gui);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("aud");
	assert(r >= 0);
	// Sound Play Channel
	r = engine->RegisterGlobalFunction("void play(const std::string &in id, arch_t channel)", WRAP_MFN_PR(audio_t, play, (const std::string&, arch_t), void), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);
	// Sound Play
	r = engine->RegisterGlobalFunction("void play(const std::string &in id)", WRAP_MFN_PR(audio_t, play, (const std::string&), void), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);
	// Sound Pause Channel
	r = engine->RegisterGlobalFunction("void pause(arch_t channel)", WRAP_MFN_PR(audio_t, pause, (arch_t), void), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);
	// Sound Resume Channel
	r = engine->RegisterGlobalFunction("void resume(arch_t channel)", WRAP_MFN_PR(audio_t, resume, (arch_t), void), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);
	// Sound Set Volume
	r = engine->RegisterGlobalFunction("void set_volume(real32_t volume)", WRAP_MFN(audio_t, set_volume), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);
	// Sound Get Volume
	r = engine->RegisterGlobalFunction("real32_t get_volume()", WRAP_MFN(audio_t, get_volume), asCALL_THISCALL_ASGLOBAL, &audio);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("pxt");
	assert(r >= 0);
	// Pxtone Load Tune
	r = engine->RegisterGlobalFunction("bool load(const std::string &in tune)", WRAP_MFN_PR(music_t, load, (const std::string&), bool), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Load Tune
	r = engine->RegisterGlobalFunction("bool load(const std::string &in tune, real32_t start, real32_t fade)", WRAP_MFN_PR(music_t, load, (const std::string&, real_t, real_t), bool), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Exit Tune
	r = engine->RegisterGlobalFunction("void exit()", WRAP_MFN(music_t, clear), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Play Tune
	r = engine->RegisterGlobalFunction("void play(real32_t start, real32_t fade)", WRAP_MFN(music_t, play), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Stop Tune
	r = engine->RegisterGlobalFunction("void pause()", WRAP_MFN(music_t, pause), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Fade Tune
	r = engine->RegisterGlobalFunction("void fade(real32_t seconds)", WRAP_MFN(music_t, fade_out), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Redo Tune
	r = engine->RegisterGlobalFunction("void redo(real32_t seconds)", WRAP_MFN(music_t, resume), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Loop Tune
	r = engine->RegisterGlobalFunction("void loop(bool state)", WRAP_MFN(music_t, set_looping), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Get Playing
	r = engine->RegisterGlobalFunction("bool playing()", WRAP_MFN(music_t, running), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Set Volume
	r = engine->RegisterGlobalFunction("void set_volume(real32_t volume)", WRAP_MFN(music_t, set_volume), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);
	// Pxtone Get Volume
	r = engine->RegisterGlobalFunction("real32_t get_volume()", WRAP_MFN(music_t, get_volume), asCALL_THISCALL_ASGLOBAL, &music);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("ktx");
	assert(r >= 0);
	// Spawn Actor
	r = engine->RegisterGlobalFunction("bool spawn(const std::string &in name, real32_t x, real32_t y, sint32_t id)", WRAP_MFN(kontext_t, create_minimally), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Generate Smoke
	r = engine->RegisterGlobalFunction("void smoke(real32_t x, real32_t y, arch_t count)", WRAP_MFN_PR(kontext_t, smoke, (real_t, real_t, arch_t), void), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Generate Shrapnel
	r = engine->RegisterGlobalFunction("void shrapnel(real32_t x, real32_t y, arch_t count)", WRAP_MFN_PR(kontext_t, shrapnel, (real_t, real_t, arch_t), void), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Kill Actor
	r = engine->RegisterGlobalFunction("void kill(sint32_t id)", WRAP_MFN(kontext_t, kill_id), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Destroy Actor
	r = engine->RegisterGlobalFunction("void destroy(sint32_t id)", WRAP_MFN(kontext_t, destroy_id), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Bump Actor
	r = engine->RegisterGlobalFunction("void move(sint32_t id, real32_t velocity_x, real32_t velocity_y)", WRAP_MFN(kontext_t, bump), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Animate Actor
	r = engine->RegisterGlobalFunction("void animate(sint32_t id, arch_t state, arch_t variation)", WRAP_MFN(kontext_t, animate), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Set Actor State
	r = engine->RegisterGlobalFunction("void set_state(sint32_t id, arch_t state)", WRAP_MFN(kontext_t, set_state), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Set Actor Flag
	r = engine->RegisterGlobalFunction("void set_flag(sint32_t id, arch_t flag, bool value)", WRAP_MFN(kontext_t, set_mask), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Set Actor Event
	r = engine->RegisterGlobalFunction("void set_event(sint32_t id, std::event@ event)", WRAP_MFN(kontext_t, set_event), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Set Actor Major Fight
	r = engine->RegisterGlobalFunction("void set_fight(sint32_t id, std::event@ event)", WRAP_MFN(kontext_t, set_fight), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);
	// Is Actor Still
	r = engine->RegisterGlobalFunction("bool still(sint32_t id)", WRAP_MFN(kontext_t, still), asCALL_THISCALL_ASGLOBAL, &kontext);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("nao");
	assert(r >= 0);
	// Visible Naomi
	r = engine->RegisterGlobalFunction("void visible(bool state)", WRAP_MFN(naomi_state_t, set_visible), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Animate Naomi
	r = engine->RegisterGlobalFunction("void animate(arch_t state, arch_t direction)", WRAP_MFN(naomi_state_t, set_sprite_animation), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Teleport Naomi
	r = engine->RegisterGlobalFunction("void teleport(real32_t x, real32_t y)", WRAP_MFN(naomi_state_t, set_teleport_location), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Heal Naomi
	r = engine->RegisterGlobalFunction("void life_up(sint32_t amount)", WRAP_MFN(naomi_state_t, boost_current_barrier), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Boost Life Naomi
	r = engine->RegisterGlobalFunction("void life_boost(sint32_t amount)", WRAP_MFN(naomi_state_t, boost_maximum_barrer), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Modify Corruption Naomi
	r = engine->RegisterGlobalFunction("void mut_leviathan(sint32_t amount)", WRAP_MFN(naomi_state_t, mut_leviathan_power), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Set Equip Naomi
	r = engine->RegisterGlobalFunction("void equip(arch_t flag, bool value)", WRAP_MFN(naomi_state_t, set_equips), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);
	// Bump Naomi
	r = engine->RegisterGlobalFunction("void bump(arch_t dir)", WRAP_MFN(naomi_state_t, bump_kinematics), asCALL_THISCALL_ASGLOBAL, &naomi_state);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("cam");
	assert(r >= 0);
	// Infinite Quake
	r = engine->RegisterGlobalFunction("void quake(real32_t factor)", WRAP_MFN_PR(camera_t, quake, (real_t), void), asCALL_THISCALL_ASGLOBAL, &camera);
	assert(r >= 0);
	// Timed Quake
	r = engine->RegisterGlobalFunction("void quake(real32_t factor, real64_t seconds)", WRAP_MFN_PR(camera_t, quake, (real_t, real64_t), void), asCALL_THISCALL_ASGLOBAL, &camera);
	assert(r >= 0);
	// Follow Actor
	r = engine->RegisterGlobalFunction("void follow(sint32_t identity)", WRAP_MFN(camera_t, follow), asCALL_THISCALL_ASGLOBAL, &camera);
	assert(r >= 0);

	// Set Namespace
	r = engine->SetDefaultNamespace("");
	assert(r >= 0);
}
