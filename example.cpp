#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "reflect_base.h"
using namespace std;

struct sctag : reflect11::object_ptr<sctag>
{
	int tag_id;
	std::string name;
	std::string icon;
	std::string jump_url;
	
	REFLECT_BEGIN()
		DECLARE_REFLECT(sctag, tag_id);
		DECLARE_REFLECT(sctag, name);
		DECLARE_REFLECT(sctag, icon);
		DECLARE_REFLECT(sctag, jump_url);
	REFLECT_END()
};

struct fcinfo : reflect11::object_ptr < fcinfo >
{
	int starttime;
	int endtime;
	std::vector<std::string> desc;
	
	REFLECT_BEGIN()
		DECLARE_REFLECT(fcinfo, starttime);
		DECLARE_REFLECT(fcinfo, endtime);
		DECLARE_REFLECT(fcinfo, desc);
	REFLECT_END()
};

struct fctag : reflect11::object_ptr<fctag>
{
	std::string name;
	std::string type;
	int tag_id;
	int calc_sub;
	int tag_type;
	std::vector<int>	tags;
	bool show;
	std::string desc1;
	fcinfo info;
	std::vector<sctag> subs;
	
	REFLECT_BEGIN()
		DECLARE_REFLECT(fctag, name);
		DECLARE_REFLECT(fctag, type);
		DECLARE_REFLECT(fctag, tag_id);
		DECLARE_REFLECT(fctag, calc_sub);
		DECLARE_REFLECT(fctag, tag_type);
		DECLARE_REFLECT(fctag, tags);
		DECLARE_REFLECT(fctag, show);
		DECLARE_REFLECT(fctag, info);
		DECLARE_REFLECT(fctag, subs);			
		DECLARE_REFLECT_DEFAULT(fctag, desc1, "default_describe");
	REFLECT_END()
};

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
bool _test_encode(int i)
{
	unsigned int encode_i = reflect11::zigzag_encode32(i);
	int decode_i = reflect11::zigzag_decode32(encode_i);

	if (decode_i != i)
	{
		printf("zigzag_encode failed %d\n", i);
		return false;
	}

	string bin;
	reflect11::varuint_encode(bin, encode_i);

	int offset = 0;
	decode_i = reflect11::varuint_decode(bin, offset);
	if (decode_i != encode_i)
	{
		printf("encode_varuint failed %d\n", i);
		return false;
	}
	return true;
}
void test_encode()
{
	for (int i = 0 ; i < 1024 * 1024 ; ++i)
	{
		if (!_test_encode(i) || !_test_encode(-i))
			return;
	}
}
int main()
{
//	test_encode();

	std::string json_data, errmsg;
	read_file("./test.json", json_data);
	json11::Json root = json11::Json::parse(json_data, errmsg);
	fctag tag;
	tag.load_json(root);
	cout << tag.dump_json() << endl;

	string bin = tag.dump_bin();

	fctag tag2;
	
	if (tag2.load_bin(bin))
	{
		cout << "load_bin ok\n-------------\n";
		cout << bin << endl;
	}
	else
	{
		cout << "load_bin error" << endl;	
	}	
	
	return 0;
}
