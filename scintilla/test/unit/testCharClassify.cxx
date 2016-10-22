// Unit Tests for Scintilla internal data structures

#include <string.h>

#include <algorithm>

#include "Platform.h"

#include "CharClassify.h"

#include "catch.hpp"

// Test CharClassify.

class CharClassifyTest {
	// Avoid warnings, private so never called. 
	CharClassifyTest(const CharClassifyTest &);
protected:
	CharClassifyTest() {
		pcc = new CharClassify();
		for (int ch = 0; ch < 256; ch++) {
			if (ch == '\r' || ch == '\n')
				charClass[ch] = CharClassify::ccNewLine;
			else if (ch < 0x20 || ch == ' ')
				charClass[ch] = CharClassify::ccSpace;
			else if (ch >= 0x80 || isalnum(ch) || ch == '_')
				charClass[ch] = CharClassify::ccWord;
			else
				charClass[ch] = CharClassify::ccPunctuation;
		}
	}

	~CharClassifyTest() {
		delete pcc;
		pcc = 0;
	}

	CharClassify *pcc;
	CharClassify::cc charClass[256];

	static const char* GetClassName(CharClassify::cc charClass) {
		switch(charClass) {
			#define CASE(c) case CharClassify::c: return #c
			CASE(ccSpace);
			CASE(ccNewLine);
			CASE(ccWord);
			CASE(ccPunctuation);
			#undef CASE
			default:
				return "<unknown>";
		}
	}
};

TEST_CASE_METHOD(CharClassifyTest, "Defaults") {
	for (int i = 0; i < 256; i++) {
		if (charClass[i] != pcc->GetClass(i))
			std::cerr
			<< "Character " << i
			<< " should be class " << GetClassName(charClass[i])
			<< ", but got " << GetClassName(pcc->GetClass(i)) << std::endl;
		REQUIRE(charClass[i] == pcc->GetClass(i));
	}
}

TEST_CASE_METHOD(CharClassifyTest, "Custom") {
	unsigned char buf[2] = {0, 0};
	for (int i = 0; i < 256; i++) {
		CharClassify::cc thisClass = CharClassify::cc(i % 4);
		buf[0] = i;
		pcc->SetCharClasses(buf, thisClass);
		charClass[i] = thisClass;
	}
	for (int i = 0; i < 256; i++) {
		if (charClass[i] != pcc->GetClass(i))
			std::cerr
			<< "Character " << i
			<< " should be class " << GetClassName(charClass[i])
			<< ", but got " << GetClassName(pcc->GetClass(i)) << std::endl;
		REQUIRE(charClass[i] == pcc->GetClass(i));
	}
}

TEST_CASE_METHOD(CharClassifyTest, "CharsOfClass") {
	unsigned char buf[2] = {0, 0};
	for (int i = 1; i < 256; i++) {
		CharClassify::cc thisClass = CharClassify::cc(i % 4);
		buf[0] = i;
		pcc->SetCharClasses(buf, thisClass);
		charClass[i] = thisClass;
	}
	for (int classVal = 0; classVal < 4; ++classVal) {
		CharClassify::cc thisClass = CharClassify::cc(classVal % 4);
		int size = pcc->GetCharsOfClass(thisClass, NULL);
		unsigned char* buffer = new unsigned char[size + 1];
		buffer[size] = '\0';
		pcc->GetCharsOfClass(thisClass, buffer);
		for (int i = 1; i < 256; i++) {
			if (charClass[i] == thisClass) {
				if (!memchr(reinterpret_cast<char*>(buffer), i, size))
					std::cerr
					<< "Character " << i
					<< " should be class " << GetClassName(thisClass)
					<< ", but was not in GetCharsOfClass;"
					<< " it is reported to be "
					<< GetClassName(pcc->GetClass(i)) << std::endl;
				REQUIRE(memchr(reinterpret_cast<char*>(buffer), i, size));
			} else {
				if (memchr(reinterpret_cast<char*>(buffer), i, size))
					std::cerr
					<< "Character " << i
					<< " should not be class " << GetClassName(thisClass)
					<< ", but was in GetCharsOfClass"
					<< " it is reported to be "
					<< GetClassName(pcc->GetClass(i)) << std::endl;
				REQUIRE_FALSE(memchr(reinterpret_cast<char*>(buffer), i, size));
			}
		}
		delete []buffer;
	}
}
