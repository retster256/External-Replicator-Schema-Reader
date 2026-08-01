#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

using namespace std;

#define QWORD unsigned long long
#define DWORD unsigned int
#define PROPERTIES 0xd0
#define EVENTS 0xe8
#define CLASS 0xb8

#define PropertyDescriptorPosition 4
#define EventDescriptorPosition 5
#define ClassDescriptorPosition 2

void* GetRobloxHandle() {
    void* hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 procinfo;
    procinfo.dwSize = sizeof(PROCESSENTRY32);
    while (Process32Next(hsnap, &procinfo)) {
        if (wcscmp(procinfo.szExeFile, L"RobloxStudioBeta.exe") == 0) {
            cout << "Found Roblox Studio" << endl;
            DWORD processId = procinfo.th32ProcessID;
            void* handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
            CloseHandle(hsnap);
            return handle;
        }
    }
    CloseHandle(hsnap);
    return nullptr;
}

struct SchemaDescriptor { //either property or enum
    void* vftable;
    void* name_ptr;
};

bool IsProbablyPointer(void* handle, uint64_t ptr)
{
    bool temp;
    return ReadProcessMemory(handle, (LPCVOID)ptr, &temp, 1, 0);
}

void* GetRealSchemaNamePtr(HANDLE handle, void* start)
{
    void* current = start;
    void* next;

    while (ReadProcessMemory(handle, current, &next, sizeof(next), nullptr))
    {
        if (!IsProbablyPointer(handle, reinterpret_cast<uint64_t>(next)))
            break;

        current = next;
    }

    return current;
}

std::string IntToHexByte(unsigned char value)
{
    const char* hex = "0123456789ABCDEF";

    std::string result;
    result += hex[(value >> 4) & 0xF];
    result += hex[value & 0xF];

    return result;
}

int main()
{
    system("title External Schema Reader");
    void* roblox_handle = GetRobloxHandle();
    QWORD network_schema = 0;
    cout << "enter network schema in base 10: ";
    cin >> network_schema;
    cout << "Using schema: ";
    cout << (void*)network_schema << endl;

    while (1) {
        char type = 'p';
        int object_size = 0;
        cout << "Enter class type p = Property, e = Event, c = Class: ";
        cin >> type;
        cout << endl;
        if (type != 'p' && type != 'e' && type != 'c') {
            cout << "invalid type" << endl;
            continue;
        }
        DWORD offset_using = 0;
        DWORD position_index = 0;
        switch (type) {
            case 'e':
                offset_using = EVENTS;
                position_index = EventDescriptorPosition;
                object_size = 5;
                break;
            case 'c':
                offset_using = CLASS;
                position_index = ClassDescriptorPosition;
                object_size = 23; //crazy right
                break;
            default:
                offset_using = PROPERTIES;
                position_index = PropertyDescriptorPosition;
                object_size = 4;
                break;
        }
        QWORD schema_address = network_schema + offset_using;
        cout << "Values at ";
        cout << (void*)schema_address << endl;

        void* address_pointer;
        if (!ReadProcessMemory(roblox_handle, (void*)schema_address, &address_pointer, 8, 0)) {
            cout << "failed read" << endl;
            continue;
        }
        //i just realised events and properties have it different. prop object_size = 4 event = 5
        SchemaDescriptor desc;
        QWORD min_value = 0x00007FF000000000;
        QWORD max_value = 0x00007FFFFFFFFFFF;
        QWORD index = 1;
        system("cls");
        cout << (void*)(address_pointer) << endl;
        string name;
        cout << "enter Name: ";
        cin >> name;
        cout << endl;

        while (1) {
      
            void* desc_val;
            DWORD offset = (index * 8 * (object_size)) - ((object_size - (position_index - 1)) * 8); //this is correct
            if (!ReadProcessMemory(roblox_handle, (void*)((QWORD)address_pointer + offset), &desc_val, 8, 0)) {
                cout << "failed read" << endl;
                break;
            }
            if ((QWORD)desc_val <= min_value or (QWORD)desc_val >= max_value) {
                cout << "bad read" << endl;
                cout << desc_val << endl;
                break;
            }
            
            if (!ReadProcessMemory(roblox_handle, desc_val, &desc, sizeof(desc), 0)) {
                cout << "failed to read object" << endl;
                break;
            }
            void* real_name_ptr = GetRealSchemaNamePtr(roblox_handle, desc.name_ptr);
            char name_buffer[256] = { 0 };
            if (!ReadProcessMemory(roblox_handle, real_name_ptr, &name_buffer, 256, 0)) {
                cout << "name read failed" << endl;
                cout << (real_name_ptr) << endl;
                index += 1;
                continue;
            };
            name_buffer[255] = 0;
            cout << name_buffer << " " << real_name_ptr << endl;

            if (strcmp(name_buffer, name.c_str()) == 0) {
                cout << "found value -> ";
                cout << name << endl;
                cout << "NetworkId -> ";
                DWORD network_id = 0;
                void* id_address = (void*)((ULONGLONG)address_pointer + (index * 8 * object_size));
                if (!ReadProcessMemory(roblox_handle, id_address, &network_id, 4, 0)) {
                    cout << "failed to read network id -> " << id_address << endl;
                }
                for (int i = 0; i < 2; ++i) {
                    cout << IntToHexByte(((char*)(&network_id))[i]).c_str() << " ";
                }
                cout << endl;
                cout << endl << endl;
                cin.get();
                break;
            }
            index += 1;
        }

    }

    return 0;
}
