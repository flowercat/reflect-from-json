# reflect-from-json
In our project, we use json11(https://github.com/dropbox/json11) to read/write json.
For general use(restful api), we can use vector/map to form json data conveniently.

Sometimes we use json as a config file to descirbe data.
when i use it , it is troublesome when i want to save it to my structs.
For instance,we need use map.find(field_name) to judge whether a field exists; 
we need use is_number, is_string... to judge whether a field data type is legal and 
some similar function to fetch its value.
If i add some field, i also need to fetch the field to my structs, it is hard to expand.

Some other language has reflect function, so, they can easily to map the field to structs.
With the same idea, i write this tools.

Currently, the atomic data only support int, bool, string as i use this to map json to structs.

1.use tools to generate data structs.
	e.g. ./reflect_tool test.json testroot

2.use json11 to parse the json document, retrieve the json11::Json object.
	call the function load_json(const json11::Json& root) 
	call the function dump_json() to check the content
	

3.If you want to use default value for the field may not exists in json, use DECLARE_REFLECT_DEFAULT
to replace the default MARCO DECLARE_REFLECT



