#pragma once
#include <cstdio>
#include <set>
#include <array>
#include <mutex>
#include <thread>
#include <atomic>
//#include <utility>
#include "vector.hpp"
//#include "pool.hpp"
#include "../shared.hpp"
#include <string_view>
#include <cstring>
#include <algorithm>
#include <memory>
#include "containers.hpp"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max



using namespace std::literals::string_view_literals;

namespace intercept {
    class sqf_functions;
    class registered_sqf_function_impl;
    class invoker;

    namespace __internal {
        class game_functions;
        class game_operators;
        class gsNular;
    }

    namespace types {
        class game_value;
        using game_value_parameter = const game_value&;
        class game_data_array;
        class internal_object;
        class game_data;

        using nular_function = game_value(*) (uintptr_t state);
        using unary_function = game_value(*) (uintptr_t state, game_value_parameter);
        using binary_function = game_value(*) (uintptr_t state, game_value_parameter, game_value_parameter);

        enum class game_data_type {
            SCALAR,
            BOOL,
            ARRAY,
            STRING,
            NOTHING,
            ANY,
            NAMESPACE,
            NaN,
            CODE,
            OBJECT,
            SIDE,
            GROUP,
            TEXT,
            SCRIPT,
            TARGET,
            CONFIG,
            DISPLAY,
            CONTROL,
            NetObject,
            SUBGROUP,
            TEAM_MEMBER,
            TASK,
            DIARY_RECORD,
            LOCATION,
            end
        };

        [[deprecated("use game_data_type")]] typedef game_data_type GameDataType;

        typedef std::set<std::string> value_types;
        typedef uintptr_t value_type;
        namespace __internal {
            void set_game_value_vtable(uintptr_t vtable);
            template <typename T, typename U>
            std::size_t pairhash(const T& first, const U& second) {
                size_t _hash = std::hash<T>()(first);
                _hash ^= std::hash<U>()(second) + 0x9e3779b9 + (_hash << 6) + (_hash >> 2);
                return _hash;
            }
            class I_debug_value {  //ArmaDebugEngine is very helpful... (No advertising.. I swear!)
            public:
                I_debug_value() = default;
                virtual ~I_debug_value() = default;
                // IDebugValue
                virtual int IAddRef() = 0;
                virtual int IRelease() = 0;
            };
        }

    #pragma region Serialization
        enum class serialization_return {
            /*
            int __cdecl sub_108BC00(int a1) {
            switch ( a1 ) {
            case 0:
            result = (int)"No error";
            break;
            case 1:
            case 7:
            result = (int)"No such file";
            break;
            case 2:
            result = (int)"Bad file (CRC, ...)";
            break;
            case 3:
            result = (int)"Bad file structure";
            break;
            case 4:
            result = (int)"Unsupported format";
            break;
            case 5:
            result = (int)"Version is too new";
            break;
            case 6:
            result = (int)"Version is too old";
            break;
            case 8:
            result = (int)"Access denied";
            break;
            case 9:
            result = (int)"Disk error";
            break;
            case 10:
            result = (int)"No entry";
            break;
            default:
            sub_10E9550("LSError");
            case 12:
            result = (int)"Unknown error";
            break;
            }
            return result;
            }
            */
            no_error,
            no_such_file,
            bad_file,
            bad_file_structure,
            unsupported_format,
            version_too_new,
            version_too_old,
            no_such_file2,
            access_denied,
            disk_error,
            no_entry,
            unknown_error = 12
        };

        class param_archive_array_entry {
        public:
            virtual ~param_archive_array_entry() {}

            virtual operator float() const { return 0; }
            virtual operator int() const { return 0; }
            virtual operator const r_string() const { return r_string(); } //There are two r_string thingies apparently.
            virtual operator r_string() const { return r_string(); }
            virtual operator bool() const { return false; }
        };

        class param_archive_entry {
        public:
            virtual ~param_archive_entry() {}

            //Generic stuff for all types
            virtual int entry_count() const { return 0; } //Number of entries. count for array and number of class entries otherwise
            virtual param_archive_entry *get_entry_by_index(int index_) const { return nullptr; } //Don't know what that's used for.
            virtual r_string current_entry_name() { return r_string(); } //Dunno exactly what this is. on GameData serialize it returns "data"
            virtual param_archive_entry *get_entry_by_name(const r_string &name) const { return nullptr; }

            //Normal Entry. Contains a single value of a single type
            virtual operator float() const { return 0; }
            virtual operator int() const { return 0; }
            virtual operator int64_t() const { return 0; }
        private:
            //https://stackoverflow.com/questions/19356232/why-is-explicit-not-compatible-with-virtual
            //https://connect.microsoft.com/VisualStudio/feedback/details/805301/explicit-cannot-be-used-with-virtual
            virtual
            #ifndef _MSC_VER
                explicit
            #endif
                operator const r_string() const { return operator r_string(); }
        public:
            virtual operator r_string() const { return r_string(); }
            virtual operator bool() const { return false; }
            virtual r_string _placeholder1(uint32_t) const { return r_string(); }

            //Array entry
            virtual void reserve(int count_) {}//Yes.. This is indeed a signed integer for something that should be unsigned...
            virtual void add_array_entry(float value_) {}
            virtual void add_array_entry(int value_) {}
            virtual void add_array_entry(int64_t value_) {}
            virtual void add_array_entry(const r_string & value_) {}
            virtual int count() const { return 0; }
            virtual param_archive_array_entry *operator [] (int index_) const { return new param_archive_array_entry(); }

            //Class entry (contains named values)
            virtual param_archive_entry *add_entry_array(const r_string &name_) { return new param_archive_entry; }
            virtual param_archive_entry *add_entry_class(const r_string &name_, bool unknown_ = false) { return new param_archive_entry; }
            virtual void add_entry(const r_string &name_, const r_string &value_) {}
            virtual void add_entry(const r_string &name_, float value_) {}
            virtual void add_entry(const r_string &name_, int value_) {}
            virtual void add_entry(const r_string &name_, int64_t value_) {}

            virtual void compress() {}
            virtual void _placeholder(const r_string &name_) {}
        };
        class serialize_class;
        class param_archive {
        public:
            static uintptr_t get_game_state();//Kinda missplaced here...
            param_archive(param_archive_entry* p1) : _p1(p1) { _parameters.push_back(get_game_state()); }
            virtual ~param_archive() { if (_p1) rv_allocator<param_archive_entry>::destroy_deallocate(_p1); }
            //#TODO add SRef class
            param_archive_entry* _p1{ rv_allocator<param_archive_entry>::create_single() }; //pointer to classEntry. vtable something
            int _version{ 1 }; //version
                               //#TODO is that 64bit on x64?
            uint32_t _p3{ 0 }; //be careful with alignment seems to always be 0 for exporting.
            auto_array<uintptr_t> _parameters; //parameters? on serializing gameData only element is pointer to gameState
            bool _isExporting{ true }; //writing data vs loading data
            serialization_return serialize(r_string name, serialize_class& value, int min_version);
            serialization_return serialize(r_string name, r_string& value, int min_version);
            serialization_return serialize(r_string name, bool& value, int min_version);
            serialization_return serialize(r_string name, bool& value, int min_version, bool default_value);
            template <class Type>
            serialization_return serialize(const r_string &name, ref<Type> &value, int min_version) {
                if (_version < min_version) return serialization_return::version_too_new;
                if (_isExporting || _p3 == 2) {
                    if (!value) return serialization_return::no_error;

                    param_archive sub_archive(nullptr);
                    sub_archive._version = _version;
                    sub_archive._p3 = _p3;
                    sub_archive._parameters = _parameters;
                    sub_archive._isExporting = _isExporting;

                    if (_isExporting) {
                        sub_archive._p1 = _p1->add_entry_class(name, false);
                    } else {
                        sub_archive._p1 = _p1->get_entry_by_name(name);
                    }

                    if (!sub_archive._p1) {
                        return serialization_return::no_entry;
                    }

                    const auto ret = value->serialize(sub_archive);
                    if (_isExporting) {
                        sub_archive._p1->compress();
                        rv_allocator<param_archive_entry>::destroy_deallocate(sub_archive._p1);
                        sub_archive._p1 = nullptr;
                    }
                } else {

                    param_archive sub_archive(nullptr);
                    sub_archive._version = _version;
                    sub_archive._p3 = _p3;
                    sub_archive._parameters = _parameters;
                    sub_archive._isExporting = _isExporting;

                    if (_isExporting) {
                        sub_archive._p1 = _p1->add_entry_class(name, false);
                    } else {
                        sub_archive._p1 = _p1->get_entry_by_name(name);
                    }

                    if (!sub_archive._p1) {
                        return serialization_return::no_entry;
                    }
                    auto val = Type::createFromSerialized(sub_archive);
                    if (val) val->serialize(sub_archive);
                    if (_isExporting) {
                        sub_archive._p1->compress();
                        rv_allocator<param_archive_entry>::destroy_deallocate(sub_archive._p1);
                        sub_archive._p1 = nullptr;
                    }
                }
                return serialization_return::no_error;
            }

        };

        class serialize_class {//That's from gameValue RTTI. I'd name it serializable_class but I think keeping it close to engine name is better.
        public:
            virtual ~serialize_class() = default;
            virtual bool _dummy(void*) { return false; }//Probably canSerialize?
            virtual serialization_return serialize(param_archive &) { return serialization_return::unknown_error; }
            virtual void _dummy2(void*) {}
        };

    #pragma endregion

        struct script_type_info {  //Donated from ArmaDebugEngine
            using createFunc = game_data* (*)(param_archive* ar);
        #ifdef __linux__
            script_type_info(r_string name, createFunc cf, r_string localizedName, r_string readableName) :
                _name(std::move(name)), _createFunction(cf), _localizedName(std::move(localizedName)), _readableName(std::move(readableName)), _javaFunc("none") {}
        #else
            script_type_info(r_string name, createFunc cf, r_string localizedName, r_string readableName, r_string description, r_string category, r_string typeName) :
                _name(std::move(name)), _createFunction(cf), _localizedName(std::move(localizedName)), _readableName(std::move(readableName)), _description(std::move(description)),
                _category(std::move(category)), _typeName(std::move(typeName)), _javaFunc("none") {}
        #endif
            r_string _name;           // SCALAR
            createFunc _createFunction{ nullptr };
            r_string _localizedName; //@STR_EVAL_TYPESCALAR
            r_string _readableName; //Number
        #ifndef __linux__
            r_string _description; //A real number.
            r_string _category; //Default
            r_string _typeName; //float/NativeObject
        #endif
            r_string _javaFunc; //Lcom/bistudio/JNIScripting/NativeObject;
        };

        struct compound_value_pair {
            script_type_info *first;
            script_type_info *second;
        };

        struct compound_script_type_info {
            uintptr_t           v_table;
            compound_value_pair *types;
        };

        class sqf_script_type : public serialize_class {
        public:
            static uintptr_t type_def; //#TODO should not be accessible
            sqf_script_type() noexcept { set_vtable(type_def); }
            sqf_script_type(const script_type_info* type) noexcept {
                single_type = type; set_vtable(type_def);
            }
            //#TODO use type_def instead
            sqf_script_type(uintptr_t vt, const script_type_info* st, compound_script_type_info* ct) noexcept :
            single_type(st), compound_type(ct) {
                set_vtable(vt);
            }
            void set_vtable(uintptr_t v) noexcept { *reinterpret_cast<uintptr_t*>(this) = v; }
            uintptr_t get_vtable() const noexcept { return *reinterpret_cast<const uintptr_t*>(this); }
            sqf_script_type(sqf_script_type&& other) noexcept :
            single_type(other.single_type), compound_type(other.compound_type) {
                set_vtable(other.get_vtable());
            }
            sqf_script_type(const sqf_script_type& other) noexcept :
                single_type(other.single_type), compound_type(other.compound_type) {
                set_vtable(other.get_vtable());
            }
            sqf_script_type& operator=(const sqf_script_type& other) noexcept {
                single_type = other.single_type;
                compound_type = other.compound_type;
                set_vtable(other.get_vtable());
                return *this;
            }
            const script_type_info*     single_type{ nullptr };
            compound_script_type_info*  compound_type{ nullptr };
            value_types type() const;
            std::string type_str() const;
            bool operator==(const sqf_script_type& other) const noexcept {
                return single_type == other.single_type && compound_type == other.compound_type;
            }
            bool operator!=(const sqf_script_type& other) const noexcept {
                return single_type != other.single_type || compound_type != other.compound_type;
            }
        };

        struct unary_operator : _refcount_vtable_dummy {
            unary_function   *procedure_addr;
            sqf_script_type   return_type;
            sqf_script_type   arg_type;
        };

        struct unary_entry {
            const char *name;
            uintptr_t procedure_ptr_addr;
            unary_operator *op;
        };

        struct binary_operator : _refcount_vtable_dummy {
            binary_function   *procedure_addr;
            sqf_script_type   return_type;
            sqf_script_type   arg1_type;
            sqf_script_type   arg2_type;
        };

        struct binary_entry {
            const char *name;
            uintptr_t procedure_ptr_addr;
            binary_operator *op;
        };

        struct nular_operator : _refcount_vtable_dummy {
            nular_function   *procedure_addr;
            sqf_script_type   return_type;
        };

        struct nular_entry {
            const char *name;
            uintptr_t procedure_ptr_addr;
            nular_operator *op;
        };

        class vm_context;
        class game_data_namespace;
        class game_variable;


        class game_state {
        public:
            types::auto_array<const types::script_type_info *> _scriptTypes;

            using game_functions = intercept::__internal::game_functions;
            using game_operators = intercept::__internal::game_operators;
            using gsNular = intercept::__internal::gsNular;

            map_string_to_class<game_functions, auto_array<game_functions>> _scriptFunctions;
            map_string_to_class<game_operators, auto_array<game_operators>> _scriptOperators;
            map_string_to_class<gsNular, auto_array<gsNular>> _scriptNulars;

            auto_array<void*, rv_allocator_local<void*, 64>> dummy1;

            class game_evaluator : public refcount {//refcounted
            public:
                //See ArmaDebugEngine
                class game_var_space : public serialize_class {
                public:
                    map_string_to_class<game_variable, auto_array<game_variable>> varspace;
                    game_var_space* parent;
                    bool dummy;
                } *varspace;

                //pointer to GameVarSpace //should be scope local variables
                int dummy1;
                bool dummy2;
                bool dummy3;
                // error enum
                //r_string error text
                //sourcedocpos see ArmaDebugEngine
            } *eval;

            game_data_namespace* varspace; //Maybe currentNamespace? is actually a ref<game_data_namespace>
            auto_array<game_data_namespace*> namespaces; //mission/parsing/... namespace
            bool dummy2;
            bool onscreen_script_errors;
            vm_context* current_context;

            //callstack item types. auto_array of struct {void* to func; r_string name}


            game_data* create_gd_from_type(const sqf_script_type& type, param_archive* ar) const {
                for (auto& it : _scriptTypes) {
                    if (types::sqf_script_type(it) == type) {
                        if (it->_createFunction) return it->_createFunction(ar);
                        return nullptr;
                    }
                }
                return nullptr;
            }
        };



        class game_data : public refcount, public __internal::I_debug_value {
            friend class game_value;
            friend class intercept::invoker;
        public:
            virtual const sqf_script_type & type() const {
            #ifdef _MSC_VER //Only on MSVC on Windows
                __debugbreak(); //If you landed here you did something wrong while implementing your custom type.
            #endif
                static sqf_script_type dummy; return dummy;
            }
            virtual ~game_data() {}
        protected:
            virtual bool get_as_bool() const { return false; }
            virtual float get_as_number() const { return 0.f; }
            virtual const r_string& get_as_string() const { static r_string dummy; dummy.clear(); return dummy; } ///Only usable on String and Code! Use to_string instead!
            virtual const auto_array<game_value>& get_as_const_array() const { static auto_array<game_value> dummy; dummy.clear(); return dummy; } //Why would you ever need this?
            virtual auto_array<game_value> &get_as_array() { static auto_array<game_value> dummy; dummy.clear(); return dummy; }
            virtual game_data *copy() const { return nullptr; }
            virtual void set_readonly(bool) {} //No clue what this does...
            virtual bool get_readonly() const { return false; }
            virtual bool get_final() const { return false; }
            virtual void set_final(bool) {} //Only on GameDataCode AFAIK
        public:
            virtual r_string to_string() const { return r_string(); }
            virtual bool equals(const game_data *) const { return false; }
            virtual const char *type_as_string() const { return "unknown"; }
            virtual bool is_nil() const { return false; }
        private:
            virtual void placeholder() const {}
            virtual bool can_serialize() { return false; }


            int IAddRef() override { return add_ref(); }
            int IRelease() override { return release(); }
        public:                               //#TODO make protected and give access to param_archive
            virtual serialization_return serialize(param_archive& ar) {
                if (ar._isExporting) {
                    sqf_script_type _type = type();
                    ar.serialize(r_string("type"sv), _type, 1);
                }

                return serialization_return::no_error;
            }
            static game_data* createFromSerialized(param_archive& ar);
        protected:
        #ifdef __linux__
        public:
        #endif
            uintptr_t get_vtable() const {
                return *reinterpret_cast<const uintptr_t*>(this);
            }
            uintptr_t get_secondary_vtable() const noexcept {
                return *reinterpret_cast<const uintptr_t*>(static_cast<const __internal::I_debug_value*>(this));
            }
        };

        class game_value_conversion_error : public std::runtime_error {
        public:
            explicit game_value_conversion_error(const std::string& _Message)
                : runtime_error(_Message) {}

            explicit game_value_conversion_error(const char* _Message)
                : runtime_error(_Message) {}
        };

        class game_value : public serialize_class {
            friend class intercept::invoker;
            friend void __internal::set_game_value_vtable(uintptr_t);
        protected:
            static uintptr_t __vptr_def; //Users should not be able to access this
        public:
            game_value() noexcept {
                set_vtable(__vptr_def);
            }
            ~game_value() noexcept {}

            void copy(const game_value& copy_) noexcept {
                set_vtable(__vptr_def); //Whatever vtable copy_ has.. if it's different then it's wrong
                data = copy_.data;
            }

            game_value(const game_value& copy_) {//I don't see any use for this.
                copy(copy_);
            }

        #pragma optimize ("ts", on)
            game_value(game_value&& move_) noexcept {
                set_vtable(__vptr_def);//Whatever vtable move_ has.. if it's different then it's wrong
                data = nullptr;
                data.swap(move_.data);
            }
        #pragma optimize( "", on )

            //Conversions
            game_value(game_data* val_) noexcept {
                set_vtable(__vptr_def);
                data = val_;
            }
            game_value(float val_);
            game_value(int val_);
            game_value(size_t val_);
            game_value(bool val_);
            game_value(const std::string &val_);
            game_value(const r_string &val_);
            game_value(std::string_view val_);
            game_value(const char* str_) : game_value(std::string_view(str_)) {}
            game_value(const std::vector<game_value> &list_);
            game_value(const std::initializer_list<game_value> &list_);
            game_value(auto_array<game_value> &&array_);
            template<class Type>
            game_value(const auto_array<Type>& array_) : game_value(auto_array<game_value>(array_.begin(), array_.end())) {
                static_assert(std::is_convertible<Type, game_value>::value);
            }
            template<class Type>
            game_value(const std::vector<Type>& array_) : game_value(auto_array<game_value>(array_.begin(), array_.end())) {
                static_assert(std::is_convertible<Type, game_value>::value);
            }
            game_value(const vector3 &vec_);
            game_value(const vector2 &vec_);
            game_value(const internal_object &internal_);

            game_value & operator = (const game_value &copy_);
            game_value & operator = (game_value &&move_) noexcept;
            game_value & operator = (float val_);
            game_value & operator = (bool val_);
            game_value & operator = (const std::string &val_);
            game_value & operator = (const r_string &val_);
            game_value & operator = (std::string_view val_);
            game_value & operator = (const char* str_) {
                return this->operator =(std::string_view(str_));
            }
            game_value & operator = (const std::vector<game_value> &list_);
            game_value & operator = (const vector3 &vec_);
            game_value & operator = (const vector2 &vec_);
            game_value & operator = (const internal_object &internal_);


            operator int() const;
            operator float() const;
            operator bool() const;
            operator std::string() const;
            operator r_string() const;
            operator vector3() const;
            operator vector2() const;

            /**
            * @brief tries to convert the game_value to an array if possible
            * @throws game_value_conversion_error {if game_value is not an array}
            */
            auto_array<game_value>& to_array() const;

            /**
            * @brief tries to convert the game_value to an array if possible and return the element at given index.
            * @throw game_value_conversion_error {if game_value is not an array}
            */
            game_value& operator [](size_t i_) const;

            /**
            * @brief tries to convert the game_value to an array if possible and return the element at given index.
            * @description If value is not an array and index==0 it returns the value. 
            * If the index is out of bounds it returns empty optional.
            */
            std::optional<game_value> get(size_t i_) const;




            uintptr_t type() const;//#TODO should this be renamed to type_vtable? and make the enum variant the default? We still want to use vtable internally cuz speed
                                   /// doesn't handle all types. Will return game_data_type::ANY if not handled
            types::game_data_type type_enum() const;

            size_t size() const;
            //#TODO implement is_null. GameDataObject's objectLink == nullptr. Same for GameDataGroup and others.
            /// @brief Returns true for uninitialized game_value's and Nil values returned from Arma
            bool is_nil() const;
            /// @brief Trying to replicate SQF isNull as good as possible. It combines isNil and isNull.
            bool is_null() const;


            bool operator==(const game_value& other) const;
            bool operator!=(const game_value& other) const;


            size_t hash() const;
            //set's this game_value to null
            void clear() { data = nullptr; }


            serialization_return serialize(param_archive& ar) override;

            ref<game_data> data;
            [[deprecated]] static void* operator new(std::size_t sz_); //Should never be used
            static void operator delete(void* ptr_, std::size_t sz_);
        #ifndef __linux__
        protected:
        #endif
            uintptr_t get_vtable() const noexcept {
                return *reinterpret_cast<const uintptr_t*>(this);
            }
        #pragma optimize ("ts", on)
            void set_vtable(uintptr_t vt) noexcept {
                *reinterpret_cast<uintptr_t*>(this) = vt;
            }
        #pragma optimize ("", on)

        };


        class game_value_static : public game_value {
        public:
            ~game_value_static();
            game_value_static(const game_value& copy) : game_value(copy) {}
            game_value_static(game_value&& move) : game_value(move) {}
            game_value_static& operator=(const game_value& copy) { data = copy.data; return *this; }
            operator game_value() const { return static_cast<game_value>(*this); }
        };

        class I_debug_variable {
            //Don't use them...
        public:
            virtual ~I_debug_variable() {}
            virtual void get_name(char *buffer, int len) const = 0;
            virtual void* get_val() const = 0;
        };

        class game_variable : public I_debug_variable {
        public:
            r_string name;
            game_value val;
            bool read_only{ false };
            game_variable() {}
            game_variable(r_string name_, game_value&& val_, bool read_only_ = false) : name(std::move(name_)), val(std::move(val_)),
                read_only(read_only_) {}
            const char *get_map_key() const { return name.c_str(); }


            void get_name(char *buffer, int len) const override {
                std::copy(name.begin(), std::min(name.begin() + static_cast<size_t>(len), name.end()),
                    compact_array<char>::iterator(buffer));
                buffer[len - 1] = 0;
            }

            void* get_val() const override {
                return val.data;
            }

        };

        class vm_context : public serialize_class {
        public:
            auto_array<void*, rv_allocator_local<void*, 64>> dummy1; //#TODO check size on x64
            bool serialenabled; //disableSerialization -> true
            void* dummyu; //VMContextBattlEyeMonitor : VMContextCallback

            //const bool is_ui_context; //no touchy
            auto_array<game_value, rv_allocator_local<game_value, 32>> scriptStack;


            class source_doc {//dedmen/ArmaDebugEngine
                uintptr_t vtable;
            public:
                r_string _fileName;
                r_string _content;
            } sdoc;

            class source_doc_pos {//dedmen/ArmaDebugEngine
                uintptr_t vtable;
            public:
                r_string _sourceFile;
                int _sourceLine;

                r_string _content;
                int _pos;
            } sdocpos;

            r_string name; //profiler might like this

            //breakOut
            r_string breakscopename;//0x258
            //throw
            game_value exception_value;//0x25c
            //breakOut
            game_value breakvalue;//0x264 

            void* d[3];
            bool dumm;
            bool dumm2; //undefined variables allowed?
            const bool scheduled; //canSuspend 0xA
            bool dumm3;//0xB
            bool dumm4;//0xC
            //throw
            bool exception_state;//0x27D
            bool break_;//0xE
            bool breakout;//0xF

        };



    #pragma region GameData Types

        class game_data_number : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            static rv_pool_allocator* pool_alloc_base;
            game_data_number() noexcept;
            game_data_number(float val_) noexcept;
            game_data_number(const game_data_number &copy_);
            game_data_number(game_data_number &&move_) noexcept;
            game_data_number& operator = (const game_data_number &copy_);
            game_data_number& operator = (game_data_number &&move_) noexcept;
            static void* operator new(std::size_t sz_);
            static void operator delete(void* ptr_, std::size_t sz_);
            float number;
            size_t hash() const {
                return __internal::pairhash(type_def, number);
            }
            //protected:
            //    static thread_local game_data_pool<game_data_number> _data_pool;
        };

        class game_data_bool : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            static rv_pool_allocator* pool_alloc_base;
            game_data_bool();
            game_data_bool(bool val_);
            game_data_bool(const game_data_bool &copy_);
            game_data_bool(game_data_bool &&move_) noexcept;
            game_data_bool& operator = (const game_data_bool &copy_);
            game_data_bool& operator = (game_data_bool &&move_) noexcept;
            static void* operator new(std::size_t sz_);
            static void operator delete(void* ptr_, std::size_t sz_);
            bool val;
            size_t hash() const { return __internal::pairhash(type_def, val); }
            //protected:
            //    static thread_local game_data_pool<game_data_bool> _data_pool;
        };

        class game_data_array : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            static rv_pool_allocator* pool_alloc_base;
            game_data_array();
            game_data_array(size_t size_);
            game_data_array(const std::vector<game_value> &init_);
            game_data_array(const std::initializer_list<game_value> &init_);
            game_data_array(auto_array<game_value> &&init_);
            game_data_array(const game_data_array &copy_);
            game_data_array(game_data_array &&move_) noexcept;
            game_data_array& operator = (const game_data_array &copy_);
            game_data_array& operator = (game_data_array &&move_) noexcept;
            ~game_data_array();
            auto_array<game_value> data;
            auto length() const { return data.count(); }
            size_t hash() const { return __internal::pairhash(type_def, data.hash()); }
            static void* operator new(std::size_t sz_);
            static void operator delete(void* ptr_, std::size_t sz_);
        };

        class game_data_string : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            static rv_pool_allocator* pool_alloc_base;
            game_data_string() noexcept;
            game_data_string(const std::string &str_);
            game_data_string(const r_string &str_);
            game_data_string(const game_data_string &copy_);
            game_data_string(game_data_string &&move_) noexcept;
            game_data_string& operator = (const game_data_string &copy_);
            game_data_string& operator = (game_data_string &&move_) noexcept;
            ~game_data_string();
            r_string raw_string;
            size_t hash() const { return __internal::pairhash(type_def, raw_string); }
            static void* operator new(std::size_t sz_);
            static void operator delete(void* ptr_, std::size_t sz_);
            //protected:
            //    static thread_local game_data_pool<game_data_string> _data_pool;
        };

        class game_data_group : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_group() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, group); }
            void *group{};
        };

        class game_data_config : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_config() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, config); }
            void *config{};
            auto_array<r_string> path;
        };

        class game_data_control : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_control() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, control); }
            void *control{};
        };

        class game_data_display : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_display() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, display); }
            void *display{};
        };

        class game_data_location : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_location() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, location); }
            void *location{};
        };

        class game_data_script : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_script() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, script); }
            void *script{};
        };

        class game_data_side : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_side() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, side); }
            void *side{};
        };

        class game_data_rv_text : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_rv_text() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, rv_text); }
            void *rv_text{};
        };

        class game_data_team_member : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_team_member() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, team); }
            void *team{};
        };

        class game_data_rv_namespace : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_rv_namespace() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, rv_namespace); }
            void *rv_namespace{};
        };

        class game_data_nothing : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_nothing() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            static size_t hash() { return 0x1337; }
        };


        class game_instruction : public refcount {
        public:
            struct sourcedocpos : public serialize_class {//See ArmaDebugEngine for more info on this
                r_string sourcefile;
                uint32_t sourceline;
                r_string content;
                uint32_t pos;

                serialization_return serialize(param_archive &ar) override {
                    return serialization_return::unknown_error;
                }

            } sdp;


            virtual bool exec(game_state& state, vm_context& t) = 0;
            virtual int stack_size(void* t) const = 0;
            //Leave this alone!
        private:
            virtual bool bfunc() const { return false; }
        public:
            virtual r_string get_name() const = 0;
        };

        class game_instruction_constant : public game_instruction {
        public:
            game_value value;
        };

        class game_data_code : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_code() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, code_string); }
            r_string code_string;
            ref<compact_array<ref<game_instruction>>> instructions;
            uint32_t _dummy1{1};
            uint32_t _dummy;
            bool is_final;
        };

        class game_data_object : public game_data {
        public:
            static uintptr_t type_def;
            static uintptr_t data_type_def;
            game_data_object() noexcept {
                *reinterpret_cast<uintptr_t*>(this) = type_def;
                *reinterpret_cast<uintptr_t*>(static_cast<I_debug_value*>(this)) = data_type_def;
            }
            size_t hash() const { return __internal::pairhash(type_def, reinterpret_cast<uintptr_t>(object ? object->object : object)); }
            struct visualState {
                //will be false if you called stuff on nullObj
                bool valid{ false };
                vector3 _aside;
                vector3 _up;
                vector3 _dir;

                vector3 _position;
                float _scale;
                float _maxScale;

                float _deltaT;
                vector3 _speed; // speed in world coordinates
                vector3 _modelSpeed; // speed in model coordinates (updated in Move())
                vector3 _acceleration;
            };
            struct visual_head_pos {
                bool valid{ false };
                vector3 _cameraPositionWorld;
                vector3 _aimingPositionWorld;
            };
        #ifndef __linux__
            //Not compatible yet. Not sure if every will be.
            visualState get_position_matrix() const noexcept {
                if (!object || !object->object) return visualState();
                const uintptr_t vbase = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(object->object) + 0xA0);

                struct visState1 {
                    vector3 _aside;
                    vector3 _up;
                    vector3 _dir;

                    vector3 _position;
                    float _scale;
                    float _maxScale;
                };
                struct visState2 {
                    float _deltaT;
                    vector3 _speed;
                    vector3 _modelSpeed;
                    vector3 _acceleration;
                };

                const auto s1 = reinterpret_cast<const visState1*>(vbase + sizeof(uintptr_t));
                const auto s2 = reinterpret_cast<const visState2*>(vbase + 0x44);

                return visualState{
                    true,
                    s1->_aside,
                    s1->_up,
                    s1->_dir,
                    { s1->_position.x,s1->_position.z,s1->_position.y },
                    s1->_scale,
                    s1->_maxScale,
                    s2->_deltaT,
                    s2->_speed,
                    s2->_modelSpeed,
                    s2->_acceleration
                };
            }

            visual_head_pos get_head_pos() const {


                if (!object || !object->object) return visual_head_pos();
            #if _WIN64 || __X86_64__
                uintptr_t vbase = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(object->object) + 0xD0);
            #else
                const auto vbase = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(object->object) + 0xA0);
            #endif
                class v1 {
                    virtual void doStuff() noexcept {}
                };
                class v2 : public v1 {
                    void doStuff() noexcept override {}
                };
                v2* v = reinterpret_cast<v2*>(vbase);
                auto& typex = typeid(*v);
            #ifdef __GNUC__
                auto test = typex.name();
            #else
                const auto test = typex.raw_name();
            #endif

                const auto hash = typex.hash_code();
                if (hash !=
                #if _WIN64 || __X86_64__
                    0xb57aedbe2fc8b61e
                #else
                    0x6d4f3e40
                #endif
                    && strcmp(test, ".?AVManVisualState@@") != 0) return  visual_head_pos();
                const auto s3 = reinterpret_cast<const visual_head_pos*>(vbase +
                #if _WIN64 || __X86_64__
                    0x168
                #else
                    0x114
                #endif
                    );
                return  visual_head_pos();
                return visual_head_pos{
                    true,
                    { s3->_cameraPositionWorld.x,s3->_cameraPositionWorld.z,s3->_cameraPositionWorld.y },
                    { s3->_aimingPositionWorld.x,s3->_aimingPositionWorld.z,s3->_aimingPositionWorld.y }
                };

            }
        #endif
            struct {
                uint32_t _x{};
                void* object{};
            } *object{};
        };
        //#TODO add game_data_nothing

        namespace __internal {
            game_data_type game_datatype_from_string(const r_string& name);
            std::string to_string(game_data_type type);
            //Not public API!
            void add_game_datatype(r_string name, game_data_type type);

            struct allocatorInfo {
                uintptr_t genericAllocBase;
                uintptr_t poolFuncAlloc;
                uintptr_t poolFuncDealloc;
                std::array<rv_pool_allocator*, static_cast<size_t>(game_data_type::end)> _poolAllocs;
                game_value(*evaluate_func) (const game_data_code&, void* ns, const r_string& name) { nullptr };
                void(*setvar_func) (const char* name, const game_value& val) { nullptr };
                uintptr_t gameState;
            };
        }


    #pragma endregion

    #pragma region RSQF
        class registered_sqf_function {
            friend class intercept::sqf_functions;
        public:
            constexpr registered_sqf_function() noexcept {}
            explicit registered_sqf_function(std::shared_ptr<registered_sqf_function_impl> func_) noexcept;
            void clear() noexcept { _function = nullptr; }
            bool has_function() const noexcept { return _function.get() != nullptr; }
        private:
            std::shared_ptr<registered_sqf_function_impl> _function;
        };

    #if defined _MSC_VER && !defined _WIN64
    #pragma warning(disable:4731) //ebp was changed in assembly
        template <game_value(*T)(game_value_parameter, game_value_parameter)>
        static game_value userFunctionWrapper(uintptr_t, game_value_parameter left_arg_, game_value_parameter right_arg_) noexcept {
            void* func = (void*) T;
            __asm {
                pop ecx;
                pop ebp;
                mov eax, [esp + 12];
                mov[esp + 8], eax;
                mov eax, [esp + 16];
                mov[esp + 12], eax;
                jmp ecx;
            }
        }

        template <game_value(*T)(game_value_parameter)>
        static game_value userFunctionWrapper(uintptr_t, game_value_parameter right_arg_) noexcept {
            void* func = (void*) T;
            __asm {
                pop ecx;
                pop ebp;
                mov eax, [esp + 12];
                mov[esp + 8], eax;
                jmp ecx;
            }
        }

        template <game_value(*T)()>
        static game_value userFunctionWrapper(uintptr_t) noexcept {
            void* func = (void*) T;
            __asm {
                pop ecx;
                pop ebp;
                jmp ecx;
            }
        }
    #pragma warning(default:4731) //ebp was changed in assembly
    #else
        template <game_value(*T)(game_value_parameter, game_value_parameter)>
        static game_value userFunctionWrapper(uintptr_t, game_value_parameter left_arg_, game_value_parameter right_arg_) noexcept {
            return T(left_arg_, right_arg_);
        }

        template <game_value(*T)(game_value_parameter)>
        static game_value userFunctionWrapper(uintptr_t, game_value_parameter right_arg_) noexcept {
            return T(right_arg_);
        }

        template <game_value(*T)()>
        static game_value userFunctionWrapper(uintptr_t) noexcept {
            return T();
        }
    #endif
    #pragma endregion

        enum class register_plugin_interface_result {
            success,
            interface_already_registered,
            interface_name_occupied_by_other_module, //Use list_plugin_interfaces(name_) to find out who registered it 
            invalid_interface_class
        };


    }
}

namespace std {
    template <> struct hash<intercept::types::r_string> {
        size_t operator()(const intercept::types::r_string& x) const {
            return x.hash();
        }
    };
    template <> struct hash<intercept::types::game_value> {
        size_t operator()(const intercept::types::game_value& x) const {
            return x.hash();
        }
    };
}


#pragma pop_macro("min")
#pragma pop_macro("max")



