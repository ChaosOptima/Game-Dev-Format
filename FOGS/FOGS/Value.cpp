#include "Value.h"
#include "Node.h"
#include "Context.h"
#include <ctype.h>
#include <assert.h>
#include <string>



namespace FOGS
{
	Allocator<sizeof(ValueItem_impl)> ValueItemAlloc;
	Allocator<sizeof(ValueData_impl)> ValueDataAlloc;

	ValueData_impl::ValueData_impl(FOGS_Context* _context)
	{
		m_Context = _context;
	}

	void ValueData_impl::ReadValue()
	{
		m_End = 0;
		*m_Context += new READER_ALLOC  Reader<ValueData_impl>(this, &ValueData_impl::ValueReader);
	}

	bool ValueData_impl::ValueReader(char* _char)
	{
		if (m_Context->CanSkip(_char))
		{
			return true;
		}
		else if (IsUnexpacted(_char))
			return false;
		else if (*_char == '\"')
			return ReadString();
		else if (*_char == '[')
			return ReadMeta();
		else if (*_char == ',')
		{
			if (m_CurrentValue && m_CurrentValue->String)
				*(m_End + 1) = 0;

			m_CurrentValue = 0;
			m_ValueReady = false;
			return true;
		}
		else if (*_char == ';')
		{
			m_Context->PopReader();
			if (m_CurrentValue && m_CurrentValue->String)
				*(m_End + 1) = 0;
			return true;
		}
		else if (*_char == ')')
		{
			if (!m_Attribute)
				return false;

			m_Context->PopReader();
			auto lv_Ret = m_Context->Read(_char);
			if (m_CurrentValue && m_CurrentValue->String)
				*(m_End + 1) = 0;
			return lv_Ret;
		}
		else if (*_char == '}')
		{
			if (!m_Node)
				return false;

			m_Context->PopReader();
			auto lv_Ret = m_Context->Read(_char);
			if (m_CurrentValue && m_CurrentValue->String)
				*(m_End + 1) = 0;
			return lv_Ret;
		}
		else if (*_char == '{')
		{
			if (!m_Node)
				return false;

			m_Context->PopReader();
			m_Node->ReadNodes();

			if (m_CurrentValue && m_CurrentValue->String)
				*(m_End + 1) = 0;
			return true;
		}
		else
		{
			if (m_ValueReady)
				return false;

			if (!m_CurrentValue)
			{
				m_CurrentValue = AddItem();
			}
			
			m_End = _char;
			if (!m_CurrentValue->String)
				m_CurrentValue->String = _char;
		}

		return true;
	}
	

	void ValueData_impl::ReadValue(char* _lable)
	{
		ReadValue();
		m_CurrentValue = AddItem();
		m_CurrentValue->Lable = _lable;
		ReadMeta();
	}

	bool ValueData_impl::IsUnexpacted(char* _char)
	{
		return
			*_char == '(' || *_char == ']' ||
			*_char == '=';
	}

	bool ValueData_impl::ReadString()
	{
		m_Context->ReadAsString(true);
		if (!m_CurrentValue)
		{
			m_CurrentValue = AddItem();
		}

		m_CurrentValue->Type = VT_STRING;

		*m_Context += new READER_ALLOC  Reader<ValueData_impl>(this, &ValueData_impl::StringReader);
		
		return true;
	}

	bool ValueData_impl::StringReader(char* _char)
	{
		if (m_IgnorNext)
		{
			m_IgnorNext = false;
			return true;
		}

		if (*_char == '\\')
			m_IgnorNext = true;
		else if (*_char == '\"')
		{
			m_Context->ReadAsString(false);
			if (m_CurrentValue->String)
				*(m_End + 1) = 0;
			m_CurrentValue = 0;
			m_ValueReady = true;
			m_Context->PopReader();
			return true;
		}

		m_End = _char;
		if (!m_CurrentValue->String)
			m_CurrentValue->String = _char;

		return true;
	}

	bool ValueData_impl::ReadMeta()
	{
		if (m_ValueReady)
			return false;

		if (!m_CurrentValue)
		{
			m_CurrentValue = AddItem();
		}
		else
		{
			if (m_CurrentValue->String)
			{
				*(m_End + 1) = 0;
				m_CurrentValue->Lable = m_CurrentValue->String;
				m_CurrentValue->String = 0;
			}
		}

		m_CurrentValue->Type = VT_META;

		m_Context->ReadAsMeta(true);
		*m_Context += new READER_ALLOC  Reader<ValueData_impl>(this, &ValueData_impl::MetaReader);

		return true;
	}

	bool ValueData_impl::MetaReader(char* _char)
	{
		if (m_IgnorNext)
		{
			m_IgnorNext = false;
			return true;
		}

		if (*_char == '\\')
			m_IgnorNext = true;
		else if (*_char == ']')
		{
			m_Context->ReadAsMeta(false);
			if (m_CurrentValue->String)
				*(m_End + 1) = 0;
			m_CurrentValue = 0;
			m_ValueReady = true;
			m_Context->PopReader();
			return true;
		}

		m_End = _char;
		if (!m_CurrentValue->String)
			m_CurrentValue->String = _char;

		return true;
	}

	ValueData_impl::~ValueData_impl()
	{
		auto lv_Val = m_Values;
		while (lv_Val)
		{
			auto lv_ForFree = lv_Val;
			lv_Val = lv_Val->Sibling;
			ValueItemAlloc.Free(lv_ForFree);
		}
	}

	ValueItem_impl* ValueData_impl::AddItem()
	{
		auto lv_NewItem = new VALUE_ITEM_ALLOC  ValueItem_impl();

		if (!m_Values)
		{
			m_Values = lv_NewItem;
			m_LastVal = lv_NewItem;
		}
		else
		{
			m_LastVal->Sibling = lv_NewItem;
			m_LastVal = lv_NewItem;
		}

		m_ValueSize++;
		return lv_NewItem;
	}

	void ValueItem_impl::FreeStrings()
	{
		if (Lable)
		{
			auto lv_size = strlen(Lable);
			auto lv_NewStr = new char[lv_size + 1];
			lv_NewStr[lv_size] = 0;
			memcpy(lv_NewStr, Lable, lv_size);
			Lable = lv_NewStr;
		}

		if (String)
		{
			auto lv_size = strlen(String);
			auto lv_NewStr = new char[lv_size + 1];
			lv_NewStr[lv_size] = 0;
			memcpy(lv_NewStr, String, lv_size);
			String = lv_NewStr;
		}

		ContexHolder = false;
	}

	ValueItem_impl::~ValueItem_impl()
	{
		if (!ContexHolder)
		{
			delete[] String;
			delete[] Lable;
		}
		
	}



	ValueData::ValueData(ValueData_impl* _data)
	{
		m_Data = _data;
	}

	ValueData::operator bool()
	{
		return m_Data->m_Values != 0;
	}

	bool ValueData::IsArray()
	{
		return m_Data->m_Values && m_Data->m_Values->Sibling;
	}

	ValueItem ValueData::Item()
	{
		if (!m_Data->m_Values)
			m_Data->AddItem();

		return m_Data->m_Values;
	}

	FOGS::ValueItem ValueData::Item(int _ind)
	{
		auto lv_Val = m_Data->m_Values;
		for (int i = 0; lv_Val && i < _ind; i++)
			lv_Val = lv_Val->Sibling;

		return lv_Val;
	}

	unsigned int ValueData::ItemsCount()
	{
		return m_Data->m_ValueSize;
	}

	FOGS::ValueItem ValueData::operator[](int _ind)
	{
		return Item(_ind);
	}

	std::vector<ValueItem> ValueData::Items()
	{
		std::vector<ValueItem> lv_Items;
		lv_Items.reserve(m_Data->m_ValueSize);
		for (auto lv_Val = m_Data->m_Values; lv_Val; lv_Val = lv_Val->Sibling)
			lv_Items.push_back(lv_Val);

		return lv_Items;
	}

	ValueData::operator ValueItem()
	{
		return m_Data->m_Values;
	}

	ValueItem ValueData::AppendItem()
	{
		auto lv_NewItem = m_Data->AddItem();
		return lv_NewItem;
	}

	ValueItem ValueData::operator*()
	{
		return Item();
	}

	bool ValueData::IsEmpty()
	{
		return !m_Data->m_Values;
	}

	//////////////////////////////////////////////////////////////////////////
	ValueItem::ValueItem(ValueItem_impl* _data)
	{
		m_Data = _data;
	}

	std::string ValueItem::Lable()
	{
		return m_Data->Lable ?m_Data->Lable :"";
	}

	ValueItem& ValueItem::Lable(const std::string& _val)
	{
		m_Data->Type = VT_META;

		auto lv_Size = _val.size();
		if (m_Data->ContexHolder)
			m_Data->FreeStrings();
		delete m_Data->Lable;

		m_Data->Lable = new char[lv_Size + 1];
		m_Data->Lable[lv_Size] = 0;
		memcpy(m_Data->Lable, _val.c_str(), lv_Size);
		return *this;
	}

	ValueType ValueItem::Type()
	{
		if (!m_Data)
			return VT_NON;

		if (m_Data->Type)
			return m_Data->Type;

		
		ValueType lv_Posible = VT_INT;
		bool DidgitPass = false;
		bool lv_EPassed = false;
		bool lv_ESignPassed = false;

		
		for (auto lv_Char = m_Data->String; lv_Char; lv_Char++)
		{
			if (lv_Posible == VT_INT)
			{
				if (!((*lv_Char >= '0' && *lv_Char <= '9') || ((*lv_Char == '-' || *lv_Char == '+') && !DidgitPass)))
				{
					if (*lv_Char == '.')
						lv_Posible = VT_FLOAT;
					else
					{
						lv_Posible = VT_CONSTANT;
						break;
					}
				}
				if ((*lv_Char == '-' || *lv_Char == '+') && !DidgitPass)
					DidgitPass = true;
			}
			else if (lv_Posible == VT_FLOAT)
			{
				if (!(*lv_Char >= '0' && *lv_Char <= '9'))
				{
					if (tolower(*lv_Char) == 'e' && !lv_EPassed)
						lv_EPassed = true;
					else if ((*lv_Char == '-' || *lv_Char == '+') && lv_EPassed && !lv_ESignPassed)
						lv_ESignPassed = true;
					else
					{
						lv_Posible = VT_CONSTANT;
						break;
					}
				}
			}
		}

		if (lv_Posible == VT_INT)
		{
			m_Data->AsInt = stoll((std::string)m_Data->String);
		}
		else if (lv_Posible == VT_FLOAT)
		{
			m_Data->AsDouble = stod((std::string)m_Data->String);
		}
		if (lv_Posible == VT_CONSTANT)
		{
			std::string lv_Tmp;
			for (auto lv_Char : (std::string)m_Data->String)
				lv_Tmp += tolower(lv_Char);

			if (lv_Tmp == "true" || lv_Tmp == "yes" || lv_Tmp == "on")
			{
				lv_Posible = VT_BOOL;
				m_Data->AsBool = true;
			}
			else if (lv_Tmp == "false" || lv_Tmp == "no" || lv_Tmp == "off")
			{
				lv_Posible = VT_BOOL;
				m_Data->AsBool = false;
			}
			else if (lv_Tmp == "pi")
			{
				lv_Posible = VT_FLOAT;
				m_Data->AsDouble = 3.141592653589;
			}
			else if (lv_Tmp == "e")
			{
				lv_Posible = VT_FLOAT;
				m_Data->AsDouble = 2.718281828459;
			}
		}

		m_Data->Type = lv_Posible;
		return m_Data->Type;
	}

	ValueItem& ValueItem::SetConstant(const std::string& _val)
	{
		m_Data->Type = VT_CONSTANT;

		auto lv_Size = _val.size();
		if (m_Data->ContexHolder)
			m_Data->FreeStrings();

		delete m_Data->String;

		m_Data->String = new char[lv_Size + 1];
		m_Data->String[lv_Size] = 0;
		memcpy(m_Data->String, _val.c_str(), lv_Size);
		return *this;
	}

	ValueItem& ValueItem::operator=(long long _val)
	{
		m_Data->Type = VT_INT;
		m_Data->AsInt = _val;
		return *this;
	}

	ValueItem& ValueItem::operator=(long double _val)
	{
		m_Data->Type = VT_FLOAT;
		m_Data->AsDouble = _val;
		return *this;
	}

	ValueItem& ValueItem::operator=(bool _val)
	{
		m_Data->Type = VT_BOOL;
		m_Data->AsBool = _val;
		return *this;
	}

	ValueItem& ValueItem::operator=(const std::string& _val)
	{
		m_Data->Type = VT_STRING;

		auto lv_Size = _val.size();
		if (m_Data->ContexHolder)
			m_Data->FreeStrings();

		delete m_Data->String;
		

		m_Data->String = new char[lv_Size + 1];
		m_Data->String[lv_Size] = 0;
		memcpy(m_Data->String, _val.c_str(), lv_Size);

		return *this;
	}

	ValueItem& ValueItem::operator=(int _val)
	{
		m_Data->Type = VT_INT;
		m_Data->AsInt = _val;
		return *this;
	}

	ValueItem& ValueItem::operator=(float _val)
	{
		m_Data->Type = VT_FLOAT;
		m_Data->AsDouble = _val;
		return *this;
	}

	ValueItem& ValueItem::operator=(const char* _val)
	{
		m_Data->Type = VT_STRING;
		auto lv_Size = strlen(_val);
		if (m_Data->ContexHolder)
			m_Data->FreeStrings();

		delete m_Data->String;

		m_Data->String = new char[lv_Size + 1];
		m_Data->String[lv_Size] = 0;
		memcpy(m_Data->String, _val, lv_Size);
		return *this;
	}

	ValueItem& ValueItem::operator=(double _val)
	{
		m_Data->Type = VT_FLOAT;
		m_Data->AsDouble = _val;
		return *this;
	}

	long long ValueItem::AsInt()
	{
		auto lv_Type = Type();
		if (lv_Type == VT_INT)
			return m_Data->AsInt;
		else if (lv_Type == VT_FLOAT)
			return (long long)m_Data->AsDouble;
		else if (lv_Type == VT_BOOL)
			return (long long)m_Data->AsBool;
		else if (lv_Type == VT_CONSTANT || lv_Type == VT_STRING || lv_Type == VT_META)
		{
			ASSERT_BREAK(false);
			return std::stoi(m_Data->String);
		}
		else
		{
			ASSERT_BREAK(false);
			return 0;
		}
	}

	long double ValueItem::AsDouble()
	{
		auto lv_Type = Type();
		if (lv_Type == VT_FLOAT)
			return m_Data->AsDouble;
		else if (lv_Type == VT_INT)
			return (long double)m_Data->AsInt;
		else if (lv_Type == VT_BOOL)
				return (long double)m_Data->AsBool;
		else if (lv_Type == VT_CONSTANT || lv_Type == VT_STRING || lv_Type == VT_META)
		{
			ASSERT_BREAK(false);
			return std::stod(m_Data->String);
		}
		else
		{
			ASSERT_BREAK(false);
			return 0;
		}
		
	}

	bool ValueItem::AsBool()
	{
		auto lv_Type = Type();
		if (lv_Type == VT_BOOL)
			return m_Data->AsBool;
		else
		{
			if (lv_Type == VT_FLOAT)
				return m_Data->AsDouble != 0;
			else if(lv_Type == VT_INT)
				return m_Data->AsInt != 0;
			else if (lv_Type == VT_CONSTANT || lv_Type == VT_STRING || lv_Type == VT_META)
				return m_Data->String != 0;
			else
				return false;
		}
	}

	std::string ValueItem::AsString()
	{
		auto lv_Type = Type();

		if (lv_Type == VT_STRING || lv_Type == VT_META || lv_Type == VT_CONSTANT)
		{
			if (m_Data->ClearString)
			{
				m_Data->ClearString = false;
				std::string lv_out;
				bool lv_ignore = false;

				for (auto lv_Char = m_Data->String; *lv_Char; lv_Char++)
				{
					if (lv_ignore)
					{
						lv_ignore = false;
						lv_out += *lv_Char;
						continue;
					}

					if (*lv_Char == '\\')
					{
						lv_ignore = true;
						continue;
					}

					if (lv_Type == VT_STRING && (*lv_Char == '\t' || *lv_Char == '\n'))
						continue;

					lv_out += *lv_Char;
				}

				auto lv_Size = lv_out.size();
				if (m_Data->ContexHolder)
					m_Data->FreeStrings();

				delete m_Data->String;
				m_Data->String = new char[lv_Size + 1];
				m_Data->String[lv_Size] = 0;
				memcpy(m_Data->String, lv_out.c_str(), lv_Size);
			}


			return m_Data->String;
		}
		else
		{
			if (lv_Type == VT_FLOAT)
			{
				return std::to_string(m_Data->AsDouble);
			}
			else if (lv_Type == VT_INT)
			{
				return std::to_string(m_Data->AsInt);
			}
			else if (lv_Type == VT_BOOL)
			{
				m_Data->String = m_Data->AsBool ? "true" : "false";
				return m_Data->String;
			}
			else
			{
				m_Data->String = 0;
				return m_Data->String;
			}
		}
	}

	ValueItem::operator int()
	{
		return (int)AsInt();
	}

	ValueItem::operator unsigned int()
	{
		return (unsigned int)AsInt();
	}

	ValueItem::operator long()
	{
		return (long)AsInt();
	}

	ValueItem::operator unsigned long()
	{
		return (unsigned long)AsInt();
	}

	ValueItem::operator unsigned long long()
	{
		return (unsigned long long)AsInt();
	}

	ValueItem::operator long long()
	{
		return AsInt();
	}

	ValueItem::operator float()
	{
		return (float)AsDouble();
	}

	ValueItem::operator double()
	{
		return AsDouble();
	}

	ValueItem::operator long double()
	{
		return AsDouble();
	}

	ValueItem::operator bool()
	{
		return AsBool();
	}

	ValueItem::operator std::string()
	{
		return AsString();
	}

	ValueItem& ValueItem::SetString(const std::string& _val)
	{
		return *this = _val;
	}

	ValueItem& ValueItem::SetInt(long long _val)
	{
		return *this = _val;
	}

	ValueItem& ValueItem::SetFloat(long double _val)
	{
		return *this = _val;
	}

	ValueItem& ValueItem::SetBool(long long _val)
	{
		return *this = _val;
	}

	


	

}


