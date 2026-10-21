#include "UpdateManager.h"
#include <QJsonArray>
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        auto require=[](bool ok){ if(!ok) throw std::runtime_error("release validation failed"); };
        QJsonObject asset{{"name","GPDF-0.2.9-Setup.exe"},{"size",12345},{"digest","sha256:"+QString(64,'a')},{"browser_download_url","https://github.com/LautaroGrant/gpdf/releases/download/v0.2.9/GPDF-0.2.9-Setup.exe"}};
        QJsonObject json{{"tag_name","v0.2.9"},{"assets",QJsonArray{asset}}};
        auto release=parseUpdateRelease(json,"0.2.8"); require(release && release->bytes==12345 && release->sha256.size()==32 && !release->installer.isEmpty());
        require(!parseUpdateRelease(json,"0.2.9")); require(!parseUpdateRelease(json,"0.3.0"));
        json["prerelease"]=true; require(!parseUpdateRelease(json,"0.2.8")); json.remove("prerelease");
        json["draft"]=true; require(!parseUpdateRelease(json,"0.2.8")); json.remove("draft");
        json["tag_name"]="v0.2.9-rc1"; require(!parseUpdateRelease(json,"0.2.8")); json["tag_name"]="v0.2.9";
        asset["browser_download_url"]="https://evil.example/GPDF-0.2.9-Setup.exe"; json["assets"]=QJsonArray{asset}; require(parseUpdateRelease(json,"0.2.8")->installer.isEmpty());
        asset["browser_download_url"]="http://github.com/LautaroGrant/gpdf/releases/download/v0.2.9/GPDF-0.2.9-Setup.exe"; json["assets"]=QJsonArray{asset}; require(parseUpdateRelease(json,"0.2.8")->installer.isEmpty());
        asset["browser_download_url"]="https://github.com/LautaroGrant/gpdf/releases/download/v0.2.9/GPDF-0.2.9-Setup.exe"; asset["digest"]="sha256:bad"; json["assets"]=QJsonArray{asset}; require(parseUpdateRelease(json,"0.2.8")->installer.isEmpty());
        asset["digest"]="sha256:"+QString(64,'a'); asset["size"]=300000000; json["assets"]=QJsonArray{asset}; require(parseUpdateRelease(json,"0.2.8")->installer.isEmpty());
        json["tag_name"]="v0.10.0"; require(parseUpdateRelease(json,"0.9.0").has_value());
        std::cout<<"PASS: version ordering, prereleases, drafts, URL, digest and download limits\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
