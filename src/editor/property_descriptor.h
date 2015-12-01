#pragma once

#include "core/blob.h"
#include "core/stack_allocator.h"
#include "core/string.h"
#include "iproperty_descriptor.h"
#include "universe\universe.h"


namespace Lumix
{


class LUMIX_EDITOR_API IIntPropertyDescriptor : public IPropertyDescriptor
{
public:
	IIntPropertyDescriptor(IAllocator& allocator);

	void setLimit(int min, int max)
	{
		m_min = min;
		m_max = max;
	}
	int getMin() const { return m_min; }
	int getMax() const { return m_max; }

private:
	int m_min;
	int m_max;
};


template <class S> class StringArrayObjectDescriptor : public IPropertyDescriptor
{
	private:
	static const int MAX_STRING_SIZE = 300;

	public:
	typedef const char* (S::*Getter)(ComponentIndex, int);
	typedef void (S::*Setter)(ComponentIndex, int, const char*);

	public:
	StringArrayObjectDescriptor(const char* name,
		Getter _getter,
		Setter _setter,
		IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = _getter;
		m_setter = _setter;
		m_type = STRING;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		char tmp[MAX_STRING_SIZE];
		char* c = tmp;
		do
		{
			stream.read(c, 1);
			++c;
		} while(*(c - 1) && (c - 1) - tmp < MAX_STRING_SIZE);
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, index, tmp);
	}


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		StackAllocator<MAX_STRING_SIZE> allocator;
		string value(allocator);
		value = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index, index);
		int len = value.length() + 1;
		stream.write(value.c_str(), len);
	}

	private:
	Getter m_getter;
	Setter m_setter;
};


template <class S>
class FileArrayObjectDescriptor : public StringArrayObjectDescriptor<S>,
								  public IFilePropertyDescriptor
{
public:
	FileArrayObjectDescriptor(const char* name,
		Getter getter,
		Setter setter,
		const char* file_type,
		IAllocator& allocator)
		: StringArrayObjectDescriptor(name, getter, setter, allocator)
		, m_file_type(file_type, m_file_type_allocator)
	{
		m_type = IPropertyDescriptor::FILE;
	}

	const char* getFileType() override { return m_file_type.c_str(); }

private:
	StackAllocator<MAX_PATH_LENGTH> m_file_type_allocator;
	string m_file_type;
};


template <class S>
class ResourceArrayObjectDescriptor : public FileArrayObjectDescriptor<S>,
									  public ResourcePropertyDescriptorBase
{
public:
	ResourceArrayObjectDescriptor(const char* name,
		Getter getter,
		Setter setter,
		const char* file_type,
		uint32 resource_type,
		IAllocator& allocator)
		: FileArrayObjectDescriptor(name, getter, setter, file_type, allocator)
		, ResourcePropertyDescriptorBase(resource_type)
	{
		m_type = IPropertyDescriptor::RESOURCE;
	}
};


template <class S> class ArrayDescriptor : public IArrayDescriptor
{
public:
	typedef int (S::*Counter)(ComponentIndex);
	typedef void (S::*Adder)(ComponentIndex, int);
	typedef void (S::*Remover)(ComponentIndex, int);

public:
	ArrayDescriptor(const char* name,
		Counter counter,
		Adder adder,
		Remover remover,
		IAllocator& allocator)
		: IArrayDescriptor(allocator)
		, m_allocator(allocator)
	{
		setName(name);
		m_type = ARRAY;
		m_counter = counter;
		m_adder = adder;
		m_remover = remover;
	}
	~ArrayDescriptor()
	{
		for (int i = 0; i < m_children.size(); ++i)
		{
			LUMIX_DELETE(m_allocator, m_children[i]);
		}
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);

		int count;
		stream.read(count);
		while (getCount(cmp) < count)
		{
			addArrayItem(cmp, -1);
		}
		while (getCount(cmp) > count)
		{
			removeArrayItem(cmp, getCount(cmp) - 1);
		}
		for (int i = 0; i < count; ++i)
		{
			for (int j = 0, cj = getChildren().size(); j < cj; ++j)
			{
				getChildren()[j]->set(cmp, i, stream);
			}
		}
	}


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		int count = getCount(cmp);
		stream.write(count);
		for (int i = 0; i < count; ++i)
		{
			for (int j = 0, cj = getChildren().size(); j < cj; ++j)
			{
				getChildren()[j]->get(cmp, i, stream);
			}
		}
	}


	int getCount(ComponentUID cmp) const override
	{
		return (static_cast<S*>(cmp.scene)->*m_counter)(cmp.index);
	}


	void addArrayItem(ComponentUID cmp, int index) const override
	{
		(static_cast<S*>(cmp.scene)->*m_adder)(cmp.index, index);
	}


	void removeArrayItem(ComponentUID cmp, int index) const override
	{
		(static_cast<S*>(cmp.scene)->*m_remover)(cmp.index, index);
	}

private:
	IAllocator& m_allocator;
	Counter m_counter;
	Adder m_adder;
	Remover m_remover;
};


template <class S> class IntPropertyDescriptor : public IIntPropertyDescriptor
{
public:
	typedef int (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, int);
	typedef int (S::*ArrayGetter)(ComponentIndex, int);
	typedef void (S::*ArraySetter)(ComponentIndex, int, int);

public:
	IntPropertyDescriptor() {}

	IntPropertyDescriptor(const char* name, Getter _getter, Setter _setter, IAllocator& allocator)
		: IIntPropertyDescriptor(allocator)
	{
		setName(name);
		m_single.getter = _getter;
		m_single.setter = _setter;
		m_type = INTEGER;
	}


	IntPropertyDescriptor(const char* name, ArrayGetter _getter, ArraySetter _setter, IAllocator& allocator)
		: IIntPropertyDescriptor(allocator)
	{
		setName(name);
		m_array.getter = _getter;
		m_array.setter = _setter;
		m_type = INTEGER;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		int32 i;
		stream.read(&i, sizeof(i));
		if(index < 0)
		{
			(static_cast<S*>(cmp.scene)->*m_single.setter)(cmp.index, i);
		}
		else
		{
			(static_cast<S*>(cmp.scene)->*m_array.setter)(cmp.index, index, i);
		}
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		int32 i = 0;
		if(index < 0)
		{
			i = (static_cast<S*>(cmp.scene)->*m_single.getter)(cmp.index);
		}
		else
		{
			i = (static_cast<S*>(cmp.scene)->*m_array.getter)(cmp.index, index);
		}
		stream.write(i);
	};

private:
	union
	{
		struct Single
		{
			Getter getter;
			Setter setter;
		} m_single;
		struct Array
		{
			ArrayGetter getter;
			ArraySetter setter;
		} m_array;
	};
};


template <class S> class StringPropertyDescriptor : public IPropertyDescriptor
{
private:
	static const int MAX_STRING_SIZE = 300;

public:
	typedef const char* (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, const char*);

public:
	StringPropertyDescriptor(const char* name, Getter getter, Setter setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = getter;
		m_setter = setter;
		m_type = IPropertyDescriptor::STRING;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		char tmp[MAX_STRING_SIZE];
		char* c = tmp;
		do
		{
			stream.read(c, 1);
			++c;
		} while(*(c - 1) && (c - 1) - tmp < MAX_STRING_SIZE);
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, tmp);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		StackAllocator<MAX_STRING_SIZE> allocator;
		string value(allocator);
		value = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = value.length() + 1;
		stream.write(value.c_str(), len);
	};


private:
	Getter m_getter;
	Setter m_setter;
};


template <class S> class BoolPropertyDescriptor : public IPropertyDescriptor
{
public:
	typedef bool (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, bool);

public:
	BoolPropertyDescriptor(const char* name, Getter getter, Setter setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = getter;
		m_setter = setter;
		m_type = IPropertyDescriptor::BOOL;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		bool b;
		stream.read(&b, sizeof(b));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, b);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		bool b = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(b);
		stream.write(&b, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
};


template <class S> class Vec3PropertyDescriptor : public IPropertyDescriptor
{
public:
	typedef Vec3 (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, const Vec3&);

public:
	Vec3PropertyDescriptor(const char* name, Getter getter, Setter setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = getter;
		m_setter = setter;
		m_type = IPropertyDescriptor::VEC3;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec3 v;
		stream.read(&v, sizeof(v));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, v);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec3 v = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(v);
		stream.write(&v, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
};


template <class S> class Vec4PropertyDescriptor : public IPropertyDescriptor
{
public:
	typedef Vec4 (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, const Vec4&);

public:
	Vec4PropertyDescriptor(const char* name, Getter getter, Setter setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = getter;
		m_setter = setter;
		m_type = IPropertyDescriptor::VEC4;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec4 v;
		stream.read(&v, sizeof(v));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, v);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec4 v = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(v);
		stream.write(&v, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
};


template <class S> class Vec2PropertyDescriptor : public IPropertyDescriptor
{
public:
	typedef Vec2 (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, const Vec2&);

public:
	Vec2PropertyDescriptor(const char* name, Getter getter, Setter setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = getter;
		m_setter = setter;
		m_type = IPropertyDescriptor::VEC2;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec2 v;
		stream.read(&v, sizeof(v));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, v);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec2 v = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(v);
		stream.write(&v, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
};


class IFilePropertyDescriptor
{
public:
	virtual ~IFilePropertyDescriptor() {}

	virtual const char* getFileType() = 0;
};


template <class T>
class FilePropertyDescriptor : public StringPropertyDescriptor<T>, public IFilePropertyDescriptor
{
public:
	FilePropertyDescriptor(const char* name,
		Getter getter,
		Setter setter,
		const char* file_type,
		IAllocator& allocator)
		: StringPropertyDescriptor(name, getter, setter, allocator)
		, m_file_type(file_type, m_file_type_allocator)
	{
		m_type = IPropertyDescriptor::FILE;
	}

	const char* getFileType() override { return m_file_type.c_str(); }

private:
	StackAllocator<MAX_PATH_LENGTH> m_file_type_allocator;
	string m_file_type;
};


template <class T>
class ResourcePropertyDescriptor : public FilePropertyDescriptor<T>,
								   public ResourcePropertyDescriptorBase
{
public:
	ResourcePropertyDescriptor(const char* name,
		Getter getter,
		Setter setter,
		const char* file_type,
		uint32 resource_type,
		IAllocator& allocator)
		: FilePropertyDescriptor(name, getter, setter, file_type, allocator)
		, ResourcePropertyDescriptorBase(resource_type)
	{
		m_type = IPropertyDescriptor::RESOURCE;
	}
};


template <class S, int COUNT> class SampledFunctionDescriptor : public ISampledFunctionDescriptor
{
public:
	typedef float (S::*Getter)(ComponentIndex, int);
	typedef void (S::*Setter)(ComponentIndex, int, float);

public:
	SampledFunctionDescriptor(const char* name,
		Getter _getter,
		Setter _setter,
		float min,
		float max,
		IAllocator& allocator)
		: ISampledFunctionDescriptor(allocator)
	{
		setName(name);
		m_getter = _getter;
		m_setter = _setter;
		m_min = min;
		m_max = max;
		m_type = SAMPLED_FUNCTION;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		for(int i = 0; i < COUNT; ++i)
		{
			float f;
			stream.read(&f, sizeof(f));
			(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, i, f);
		}
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		for(int i = 0; i < COUNT; ++i)
		{
			float f = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index, i);
			int len = sizeof(f);
			stream.write(&f, len);
		}
	};


	float getMin() override { return m_min; }
	float getMax() override { return m_max; }

private:
	Getter m_getter;
	Setter m_setter;
	float m_min;
	float m_max;
};


template <class S> class DecimalPropertyDescriptor : public IDecimalPropertyDescriptor
{
public:
	typedef float (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, float);
	typedef float (S::*ArrayGetter)(ComponentIndex, int);
	typedef void (S::*ArraySetter)(ComponentIndex, int,float);

public:
	DecimalPropertyDescriptor(const char* name,
		Getter _getter,
		Setter _setter,
		float min,
		float max,
		float step,
		IAllocator& allocator)
		: IDecimalPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = _getter;
		m_setter = _setter;
		m_array_getter = nullptr;
		m_array_setter = nullptr;
		m_min = min;
		m_max = max;
		m_step = step;
		m_type = DECIMAL;
	}


	DecimalPropertyDescriptor(const char* name,
		ArrayGetter _getter,
		ArraySetter _setter,
		float min,
		float max,
		float step,
		IAllocator& allocator)
		: IDecimalPropertyDescriptor(allocator)
	{
		setName(name);
		m_array_getter = _getter;
		m_array_setter = _setter;
		m_getter = nullptr;
		m_setter = nullptr;
		m_min = min;
		m_max = max;
		m_step = step;
		m_type = DECIMAL;
	}



	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		float f;
		stream.read(&f, sizeof(f));
		if(index >= 0)
		{
			(static_cast<S*>(cmp.scene)->*m_array_setter)(cmp.index, index, f);
		}
		else
		{
			(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, f);
		}
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		float f = 0;
		if(index >= 0)
		{
			(static_cast<S*>(cmp.scene)->*m_array_getter)(cmp.index, index);
		}
		else
		{
			(static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		}
		int len = sizeof(f);
		stream.write(&f, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
	ArrayGetter m_array_getter;
	ArraySetter m_array_setter;
};


template <class S> class ColorPropertyDescriptor : public IPropertyDescriptor
{
public:
	typedef Vec3 (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, const Vec3&);

public:
	ColorPropertyDescriptor(const char* name, Getter _getter, Setter _setter, IAllocator& allocator)
		: IPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = _getter;
		m_setter = _setter;
		m_type = COLOR;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec3 f;
		stream.read(&f, sizeof(f));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, f);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		Vec3 f = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(f);
		stream.write(&f, len);
	};

private:
	Getter m_getter;
	Setter m_setter;
};


template <class S> class EnumPropertyDescriptor : public IEnumPropertyDescriptor
{
public:
	typedef int (S::*Getter)(ComponentIndex);
	typedef void (S::*Setter)(ComponentIndex, int);
	typedef int (S::*EnumCountGetter)() const;
	typedef const char* (S::*EnumNameGetter)(int);

public:
	EnumPropertyDescriptor(const char* name,
		Getter _getter,
		Setter _setter,
		EnumCountGetter count_getter,
		EnumNameGetter enum_name_getter,
		IAllocator& allocator)
		: IEnumPropertyDescriptor(allocator)
	{
		setName(name);
		m_getter = _getter;
		m_setter = _setter;
		m_enum_count_getter = count_getter;
		m_enum_name_getter = enum_name_getter;
		m_type = ENUM;
	}


	void set(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		ASSERT(index == -1);
		int value;
		stream.read(&value, sizeof(value));
		(static_cast<S*>(cmp.scene)->*m_setter)(cmp.index, value);
	};


	void get(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		ASSERT(index == -1);
		int value = (static_cast<S*>(cmp.scene)->*m_getter)(cmp.index);
		int len = sizeof(value);
		stream.write(&value, len);
	};


	int getEnumCount(IScene* scene) override { return (static_cast<S*>(scene)->*m_enum_count_getter)(); }


	const char* getEnumItemName(IScene* scene, int index) override
	{
		return (static_cast<S*>(scene)->*m_enum_name_getter)(index);
	}

private:
	Getter m_getter;
	Setter m_setter;
	EnumCountGetter m_enum_count_getter;
	EnumNameGetter m_enum_name_getter;

};


} // !namespace Lumix
