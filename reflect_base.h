#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include "json11.hpp"
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <assert.h>

namespace reflect11
{
	enum evalue_type
	{
		VT_INT = 0,
		VT_BOOL,
		VT_STRING,
		VT_OBJECT,
		VT_ARRAY
	};

	inline std::string pad_tab(int n)
	{
		std::string ret;
		while (n--) ret += "  ";
		return ret;
	}
	// zigzag encode like this sequence 0,-1,1,-2,2....
	inline unsigned int zigzag_encode32(int n)
	{
		return (n >> 31) ^ (n << 1);
	}
	inline int zigzag_decode32(unsigned int n)
	{
		return (n >> 1) ^ (-(n & 1));
	}
	inline void varuint_encode(std::string& bin, unsigned int n)
	{
		if (n == 0)
		{
			bin.push_back((char)0);
			return;
		}
		while (n > 0x7f)
		{
			char bit = 0x80 | (n & 0x7f);
			n >>= 7;
			bin.push_back(bit);
		}
		if (n)
		{
			bin.push_back(n & 0x7f);
		}
	}	
	inline unsigned int varuint_decode(const std::string& bin, int& offset)
	{
		unsigned int ret = 0;

		int len = (int)bin.length();
		for (int i = offset, j = 0 ; i < len ; ++i, ++j)
		{
			char bit = bin[i];
			ret |= (bit & 0x7f) << (j * 7);

			if ((bit & 0x80) == 0)
			{
				offset = i + 1;
				break;
			}
		}	
		return ret;
	}
	inline const char* get_bin(const std::string& bin, int& offset, int expect_length)
	{
		int left = bin.length() - offset;
		if (left < expect_length) return nullptr;

		const char* p = bin.c_str() + offset;
		offset += expect_length;
		return p;
	}
	inline void write_type(std::string& bin, evalue_type evt)
	{
		char vt = evt;
		bin.append(&vt, sizeof(vt));
	}
	inline bool is_type(const std::string& bin, int& offset, evalue_type evt)
	{
		const char* p = get_bin(bin, offset, 1);	
		if (p == nullptr) return false;
		return *p == evt;
	}
	inline void write_int(std::string& bin, int value)
	{
		write_type(bin, VT_INT);
		unsigned int zigzag_value = zigzag_encode32(value);
		varuint_encode(bin, zigzag_value);
	}
	inline bool read_int(const std::string& bin, int& offset, int& value)
	{
		if (!is_type(bin, offset, VT_INT))
			return false;
		unsigned int zigzag_value = varuint_decode(bin, offset);
		value = zigzag_decode32(zigzag_value);
		return true;
	}
	inline void write_bool(std::string& bin, bool value)
	{
		write_type(bin, VT_BOOL);
		bin.append((char*)&value, sizeof(value));
	}
	inline bool read_bool(const std::string& bin, int& offset, bool& value)
	{
		if (!is_type(bin, offset, VT_BOOL))
			return false;

		bool* p = (bool*)get_bin(bin, offset, sizeof(bool));
		if (p == nullptr) return false;

		value = *p;
		return true;
	}
	inline void write_string(std::string& bin, const std::string& value)
	{
		write_type(bin, VT_STRING);
		write_int(bin, (int)value.length());
		bin.append(value.c_str(), value.length());
	}
	inline bool read_string(const std::string& bin, int& offset, std::string& value)
	{
		if (!is_type(bin, offset, VT_STRING))
			return false;
		
		int len = 0;
		if (!read_int(bin, offset, len))
			return false;

		const char* p = get_bin(bin, offset, len);
		if (p == nullptr) return false;

		value.assign(p, len);
		return true;
	}

	struct field_ptr
	{
		virtual bool set_field(const json11::Json& root, void* p) = 0;		
		virtual bool set_field(const std::string& bin, int& offset, void* p) = 0;

		virtual void dump_json(std::string& content, void* p, int depth) = 0;
		virtual void dump_bin(std::string& bin, void* p) = 0;
	
		virtual bool use_default(void* p) { return false; }
		virtual bool has_default() { return false; }
	};
	
	template<typename T>
	struct value_traits;
	
	template<typename C, typename T>
	struct value_ptr : field_ptr
	{
		value_ptr(T C::*mem)
			: mem_(mem)
		{}
	
		bool set_field(const json11::Json& root, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			return value_traits<T>::set_field(root, &(c->*mem_));
		}
		bool set_field(const std::string& bin, int& offset, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			return value_traits<T>::set_field(bin, offset, &(c->*mem_));
		}
	
		void dump_json(std::string& content, void* p, int depth)
		{
			C* c = reinterpret_cast<C*>(p);
			T& obj = (c->*mem_);
			return value_traits<T>::dump_json(content, &obj, depth);
		}
		void dump_bin(std::string& bin, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			T& obj = (c->*mem_);
			return value_traits<T>::dump_bin(bin, &obj);
		}
		T C::*mem_;
	};
	
	template<typename C, typename T>
	struct default_value_ptr : value_ptr<C, T>
	{
		default_value_ptr(T C::*mem, const T& default_value)
			: value_ptr<C, T>(mem)
			, default_value_(default_value)
		{}
		bool has_default() { return true; }
		bool use_default(void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			(c->*(this->mem_)) = default_value_;
			return true;
		}
		T default_value_;
	};
	
	
	template<typename C, typename T>
	struct array_ptr : field_ptr
	{
		array_ptr(std::vector<T> C::*values)
			: values_(values)
		{
	
		}
		bool set_field(const json11::Json& root, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			if (!root.is_array())
			{
				return false;
			}
	
			T obj;
			for (const auto& v : root.array_items())
			{	
				value_traits<T>::set_field(v, &obj);
				(c->*values_).push_back(obj);
			}
			return true;
		}		
		bool set_field(const std::string& bin, int& offset, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			if (!is_type(bin, offset, VT_ARRAY))
			{
				return false;
			}

			int count = 0;
			if (!read_int(bin, offset, count))
			{
				return false;
			}

			T obj;
			for (unsigned int i = 0 ; i < count ; ++i)
			{
				value_traits<T>::set_field(bin, offset, &obj);
				(c->*values_).push_back(obj);
			}
			return true;
		}
		void dump_json(std::string& content, void* p, int depth)
		{
			C* c = reinterpret_cast<C*>(p);
			std::vector<T>& values = c->*values_;
	
			std::stringstream ss;
			ss << "[";
			
			int index = 0;			
			for (auto& v : values)
			{
				std::string value_content;
				value_traits<T>::dump_json(value_content, (void*)&v, depth + 1);
				ss << value_content;
				if (index++ != values.size() - 1) ss << ",";
			}
			ss << "]";
			content += ss.str();
		}
		void dump_bin(std::string& bin, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			std::vector<T>& values = c->*values_;
			
			write_type(bin, VT_ARRAY);
			write_int(bin, (int)values.size());

			for (auto& v : values)
			{
				value_traits<T>::dump_bin(bin, (void*)&v);
			}
		}
		std::vector<T> C::*values_;
	};
	
	template<typename C>
	struct object_ptr : field_ptr
	{
		bool set_field(const json11::Json& root, void* p)
		{
			if (!root.is_object())
			{
				return false;
			}
	
			for (const auto& kv : member_addr_)
			{
				auto it = root.object_items().find(kv.first);
				if (it == root.object_items().end())
				{
					if (kv.second->use_default(p)) continue;						
					return false;
				}
				kv.second->set_field(it->second, p);
			}
			return true;
		}
		bool set_field(const std::string& bin, int& offset, void* p)
		{
			C* c = reinterpret_cast<C*>(p);
			if (!is_type(bin, offset, VT_OBJECT))
			{
				return false;
			}

			int count = 0;
			if (!read_int(bin, offset, count))
			{
				return false;
			}

			for (int i = 0 ; i < count ; ++i)
			{
				std::string key;
				if (!read_string(bin, offset, key))
				{
					return false;
				}
				auto it = member_addr_.find(key);
				if (it == member_addr_.end())
				{
					return false;
				}
				if (!it->second->set_field(bin, offset, p))
				{
					return false;
				}
			}
			return true;
		}
		std::string dump_json()
		{
			std::string content;
			dump_json(content, this, 0);
			return content;
		}
		bool load_json(const json11::Json& json_root)
		{
			return set_field(json_root, this);
		}
		void dump_json(std::string& content, void* p, int depth)
		{
			std::stringstream ss;
			ss << (depth == 0 ? "" : "\n") << pad_tab(depth) << "{\n";			
			
			int index = 0;
			for (const std::string& key : ordered_key_)
			{					
				auto it = member_addr_.find(key);
				assert(it != member_addr_.end());
	
				const auto& kv = *it;				
				//if (kv.second->has_default()) continue;
	
				ss << pad_tab(depth + 1) << "\"" << kv.first << "\":";
				std::string value_content;
				kv.second->dump_json(value_content, p, depth + 1);
				ss << value_content;
	
				if (index++ != member_addr_.size() - 1) ss << ",";
				ss << "\n";
			}
			
			ss << pad_tab(depth) << "}";
			content += ss.str();
		}
		bool load_bin(const std::string& bin)
		{
			int offset = 0;
			return set_field(bin, offset, this);
		}
		std::string dump_bin()
		{
			std::string bin;
			dump_bin(bin, this);
			return bin;
		}
		void dump_bin(std::string& bin, void* p)
		{
			write_type(bin, VT_OBJECT);

			write_int(bin, (int)member_addr_.size());

			for (const auto& kv: member_addr_)
			{
				write_string(bin, kv.first);
				kv.second->dump_bin(bin, p);
			}
		}

		template<typename T>
		static void add_member_reflect(const char* name, T C::*mem_ptr)
		{
			member_addr_.insert({ name, value_traits<T>::get_ptr(mem_ptr) });
			ordered_key_.push_back(name);
		}
		template<typename T, typename V>
		static void add_member_reflect(const char* name, T C::*mem_ptr, const V& default_value)
		{
			member_addr_.insert({ name, value_traits<T>::get_ptr(mem_ptr, T(default_value)) });
			ordered_key_.push_back(name);
		}
	
		object_ptr()
		{
			class_ref_.fetch_add(1);
			if (!member_addr_.empty()) return;
			// MT need lock.
			(static_cast<C*>(this))->do_init();
		}
		object_ptr(const object_ptr& rhs)
		{
			class_ref_.fetch_add(1);
		}
		virtual ~object_ptr()
		{
			if (class_ref_.fetch_sub(1) != 1)
			{
				return;
			}
			for (const auto& kv : member_addr_)
			{
				delete kv.second;
			}
			member_addr_.clear();
		}
		static std::map<std::string, field_ptr*> member_addr_;
		static std::vector<std::string> ordered_key_;
	
		static std::atomic<int> class_ref_;
	};
	template<typename C> std::map<std::string, field_ptr*>  object_ptr<C>::member_addr_;
	template<typename C> std::vector<std::string>  object_ptr<C>::ordered_key_;
	template<typename C> std::atomic<int> object_ptr<C>::class_ref_(0);
	
	#define REFLECT_BEGIN() void do_init(){
	#define DECLARE_REFLECT(classname, mem) add_member_reflect(#mem, &classname::mem)
	#define DECLARE_REFLECT_DEFAULT(classname, mem, default_value) add_member_reflect(#mem, &classname::mem, default_value)
	#define REFLECT_END() }
	
	template<typename T>
	struct value_traits
	{
		template<typename C>
		static field_ptr* get_ptr(T C::*mem_ptr)
		{
			return new value_ptr<C, T>(mem_ptr);
		}
		template<typename C>
		static field_ptr* get_ptr(T C::*mem_ptr, const T& default_value)
		{
			return new value_ptr<C, T>(mem_ptr);
		}
		static bool set_field(const json11::Json& root, T* value)
		{
			object_ptr<T> ptr;
			return ptr.set_field(root, value);
		}
		static bool set_field(const std::string& bin, int& offset, T* value)
		{
			object_ptr<T> ptr;
			return ptr.set_field(bin, offset, value);
		}
		static void dump_json(std::string& content, void* p, int depth)
		{
			object_ptr<T> ptr;
			return ptr.dump_json(content, p, depth);
		}
		static void dump_bin(std::string& bin, void* p)
		{
			object_ptr<T> ptr;
			return ptr.dump_bin(bin, p);
		}
	};
	
	template<typename T>
	struct value_traits<std::vector<T>>
	{
		template<typename C>
		static field_ptr* get_ptr(std::vector<T> C::*mem_ptr)
		{
			return new array_ptr<C, T>(mem_ptr);
		}
		template<typename C>
		static field_ptr* get_ptr(T C::*mem_ptr, const T& default_value)
		{
			return new array_ptr<C, T>(mem_ptr);
		}
	};
	#define ATOM_TRAITS_IMPL(atomic_t)\
		template<typename C>\
		static field_ptr* get_ptr(atomic_t C::*mem_ptr)\
		{\
			return new value_ptr<C, atomic_t>(mem_ptr);\
		}\
		template<typename C>\
		static field_ptr* get_ptr(atomic_t C::*mem_ptr, const atomic_t& default_value)\
		{\
			return new default_value_ptr<C, atomic_t>(mem_ptr, default_value);\
		}
	
	template<>
	struct value_traits <int>
	{		
		ATOM_TRAITS_IMPL(int)
		static bool set_field(const json11::Json& root, int* value)
		{
			if (!root.is_number()) return false;
			*value = (int)root.number_value();
			return true;
		}
		static bool set_field(const std::string& bin, int& offset, int* value)
		{
			return read_int(bin, offset, *value);
		}	
		static void dump_json(std::string& content, void* p, int depth)
		{
			int* c = reinterpret_cast<int*>(p);
			std::stringstream ss;
			ss << *c;
			content += ss.str();
		}
		static void dump_bin(std::string& bin, void* p)
		{
			write_int(bin, *(int*)p);
		}
	};
	
	template<>
	struct value_traits < bool >
	{
		ATOM_TRAITS_IMPL(bool)
		static bool set_field(const json11::Json& root, bool* value)
		{
			if (!root.is_bool()) return false;
			*value = root.bool_value();
			return true;
		}
		static bool set_field(const std::string& bin, int& offset, bool* value)
		{
			return read_bool(bin, offset, *value);
		}	
		static void dump_json(std::string& content, void* p, int depth)
		{
			bool* c = reinterpret_cast<bool*>(p);
			std::stringstream ss;
			ss << (*c ? "true" : "false");
			content += ss.str();
		}
		static void dump_bin(std::string& bin, void* p)
		{
			write_bool(bin, *(bool*)p);
		}
	};
	
	template<>
	struct value_traits <std::string>
	{
		ATOM_TRAITS_IMPL(std::string)
		static bool set_field(const json11::Json& root, std::string* value)
		{
			if (!root.is_string()) return false;
			*value = root.string_value();
			return true;
		}		
		static bool set_field(const std::string& bin, int& offset, std::string* value)
		{
			return read_string(bin, offset, *value);
		}
		static void dump_json(std::string& content, void* p, int depth)
		{
			std::string* c = reinterpret_cast<std::string*>(p);
			std::stringstream ss;
			ss << "\"" << *c << "\"";
			content += ss.str();
		}
		static void dump_bin(std::string& bin, void* p)
		{
			write_string(bin, *(std::string*)p);
		}
	};
}	
