#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "reflect_base.h"
#include <iostream>
using namespace std;


int read_file(const std::string& file_path, std::string& content)
{
	FILE* fp = fopen(file_path.c_str(), "rb");
	if (fp == nullptr)
	{
		return -1;
	}
	char buf[4096];
	while (true)
	{
		size_t count_read = fread(buf, sizeof(char), sizeof(buf), fp);
		if (count_read == 0)
		{
			if (!feof(fp))
			{
				fclose(fp);
				return -1;
			}
			break;
		}
		content.append(buf, count_read);
	}
	fclose(fp);
	return 0;
}

struct class_dump 
{
	vector<string> class_list_;

	string tab(int n)
	{
		string ret;
		while (n--) ret += "  ";
		return ret;
	}
			
	void dump_member(const std::map<std::string, json11::Json>& obj, const string& classname, string& content)
	{
		content += "{\n";
		for (const auto& kv : obj)
		{
			if (kv.second.is_number())
			{
				content += tab(1) + "int " + kv.first + ";\n";
			}
			else if (kv.second.is_bool())
			{
				content += tab(1) + "bool " + kv.first + ";\n";
			}
			else if (kv.second.is_string())
			{
				content += tab(1) + "std::string " + kv.first + ";\n";
			}
			else if (kv.second.is_array())
			{
				string subname = dump_obj(kv.second.array_items()[0], kv.first);
				content += tab(1) + "std::vector<" + subname + "> " + kv.first + ";\n";
			}
			else if (kv.second.is_object())
			{
				string subname = dump_obj(kv.second, kv.first);
				content += tab(1) + subname + " " + kv.first + ";\n";
			}
		}
		content += tab(1) + "REFLECT_BEGIN()\n";
		for (const auto& kv : obj)
		{				
			content += tab(2) + "DECLARE_REFLECT(" + classname + ", " + kv.first + ")\n";
		}
		content += tab(1) + "REFLECT_END()\n};\n";
	}
	string dump_obj(const json11::Json& root, const string& classname)
	{
		if (root.is_object())
		{
			string sname = "s" + classname;
			string content = "struct " + sname + " : public reflect11::object_ptr<" + sname + ">\n";
			dump_member(root.object_items(), sname, content);
			
			class_list_.push_back(content);
			return sname;
		}
		
		if (root.is_number()) return "int";
		if (root.is_bool()) return "bool";				
		if (root.is_string()) return "std::string";
		
		return "";
	}
	string dump(json11::Json& root, const char* root_class)
	{
		dump_obj(root, root_class);

		string class_desc;
		for (const auto& c : class_list_)
		{
			class_desc += c;
		}
		return class_desc;
	}
};

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("program json_path class_name\n");
		exit(0);
	}


	std::string json_data, errmsg;
	read_file(argv[1], json_data);
	json11::Json root = json11::Json::parse(json_data, errmsg);
	if (!errmsg.empty())
	{
		printf("%s", errmsg.c_str());
		exit(0);
	}

	class_dump cd;
	cout << cd.dump(root, argv[2]) << endl;
	return 0;
}
