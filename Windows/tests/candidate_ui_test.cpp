#include "../src/CandidateUI.h"
#include <cassert>
#include <iostream>
namespace fuyi { HINSTANCE module=nullptr; std::atomic<LONG> objects{0}; }
int main() {
    using namespace fuyi;
    auto ui=new CandidateUI;
    assert(ui->update({{L"你好",5},{L"拟好",5},{L"你",2},{L"好",3},{L"您好",5},{L"你们好",5}},5));
    UINT count=0,index=0;
    assert(ui->GetCount(&count)==S_OK && count==6);
    BSTR text=nullptr;
    assert(ui->GetString(5,&text)==S_OK && std::wstring(text)==L"你们好");SysFreeString(text);
    assert(ui->GetString(6,&text)==E_INVALIDARG && !text);
    assert(ui->GetPageIndex(nullptr,0,&count)==S_FALSE && count==2);
    UINT pages[2]{};assert(ui->GetPageIndex(pages,2,&count)==S_OK && pages[1]==5);
    assert(ui->GetCurrentPage(&index)==S_OK && index==1);
    UINT invalid[]={0,6};assert(ui->SetPageIndex(invalid,2)==E_INVALIDARG);
    bool committed=false,hidden=false;
    ui->select=[&](UINT selected,bool finalize){index=selected;committed=finalize;return S_OK;};
    ui->visibility=[&](bool shown){hidden=!shown;};
    assert(ui->SetSelection(1)==S_OK && index==1);
    assert(ui->Finalize()==S_OK && committed);
    assert(ui->Show(FALSE)==S_OK && hidden);
    tsf::IntegratedCandidates *integrated=nullptr;
    assert(ui->QueryInterface(tsf::integratedId,(void**)&integrated)==S_OK);
    int style=-1;assert(integrated->GetSelectionStyle(&style)==S_OK && style==0);
    assert(integrated->SetIntegrationStyle(tsf::searchStyle)==S_OK);
    IUnknown *identity=nullptr;assert(integrated->QueryInterface(IID_IUnknown,(void**)&identity)==S_OK);
    assert(identity==(IUnknown*)(tsf::CandidateBehavior*)ui);identity->Release();integrated->Release();
    ui->disconnect();assert(ui->Abort()==E_ABORT);
    assert(ui->GetCount(&count)==S_OK && count==0);
    ui->Release();assert(objects==0);
    std::cout<<"TSF candidate interfaces, paging, selection, visibility and lifetime passed\n";
}
