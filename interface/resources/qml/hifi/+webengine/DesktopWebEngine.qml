import QtQuick 2.7
import QtQuick.Dialogs 1.2 as OriginalDialogs
import QtWebEngine 1.5

Item {
    id: root

    property bool webViewProfileSetup: false
    property string currentUrl: ""
    property string downloadUrl: ""
    property string adaptedPath: ""
    property string tempDir: ""
    function setupWebEngineSettings() {
        WebEngine.settings.javascriptCanOpenWindows = true;
        WebEngine.settings.javascriptCanAccessClipboard = false;
        WebEngine.settings.spatialNavigationEnabled = false;
        WebEngine.settings.localContentCanAccessRemoteUrls = true;
    }


    function initWebviewProfileHandlers(profile) {
        downloadUrl = currentUrl;
        if (webViewProfileSetup) return;
        webViewProfileSetup = true;

        profile.downloadRequested.connect(function(download){
            // a page can start a download by itself: the user says yes to each one
            var question = desktop.messageBox({
                icon: OriginalDialogs.StandardIcon.Question,
                buttons: OriginalDialogs.StandardButton.Yes | OriginalDialogs.StandardButton.No,
                defaultButton: OriginalDialogs.StandardButton.No,
                text: "Download " + download.url + "?"
            });
            if (question.exec() !== OriginalDialogs.StandardButton.Yes) {
                download.cancel();
                return;
            }
            adaptedPath = File.convertUrlToPath(downloadUrl);
            tempDir = File.getTempDir();
            download.path = tempDir + "/" + adaptedPath;
            download.accept();
            if (download.state === WebEngineDownloadItem.DownloadInterrupted) {
                console.log("download failed to complete");
            }
        })

        profile.downloadFinished.connect(function(download){
            if (download.state === WebEngineDownloadItem.DownloadCompleted) {
                File.runUnzip(download.path, downloadUrl, autoAdd);
            } else {
                console.log("The download was corrupted, state: " + download.state);
            }
            autoAdd = false;
        })
    }
}
