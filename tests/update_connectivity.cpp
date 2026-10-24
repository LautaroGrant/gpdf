#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <iostream>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    std::cout<<"TLS backends: "<<QSslSocket::availableBackends().join(", ").toStdString()<<std::endl;
    if(!QSslSocket::supportsSsl()) { std::cerr<<"FAIL: HTTPS unavailable in the deployed runtime\n"; return 1; }
    if(!app.arguments().contains("--live")) { std::cout<<"PASS: deployed HTTPS support\n"; return 0; }
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl("https://api.github.com/repos/LautaroGrant/gpdf/releases/latest"));
    request.setRawHeader("User-Agent","GPDF-connectivity-test");
    request.setRawHeader("Accept","application/vnd.github+json");
    request.setTransferTimeout(30000);
    auto reply=network.get(request);
    QTimer::singleShot(35000,reply,&QNetworkReply::abort);
    QObject::connect(reply,&QNetworkReply::finished,&app,[&]{
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto release=QJsonDocument::fromJson(reply->readAll()).object().value("tag_name").toString();
        std::cout<<"HTTP "<<status<<"; "<<reply->errorString().toStdString()<<"; release="<<release.toStdString()<<std::endl;
        app.exit(reply->error()==QNetworkReply::NoError && status==200 && !release.isEmpty() ? 0 : 1);
    });
    return app.exec();
}
