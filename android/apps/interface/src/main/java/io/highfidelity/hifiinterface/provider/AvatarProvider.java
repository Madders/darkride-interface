package io.highfidelity.hifiinterface.provider;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import io.highfidelity.hifiinterface.HifiUtils;
import io.highfidelity.hifiinterface.view.AvatarAdapter;

/**
 * The avatars to choose from: the user's own, from the "avatars" folder of their Darkride files (where the avatar
 * importer stores them), then the built-in ones in avatars.json.
 *
 * Darkride's metaverse lists the user's files at /api/user/files as {"url": base URL, "files": [{"key": path}]},
 * and a file is at base URL + key, as the desktop avatar app uses them. An avatar is a model file in the avatars
 * folder (an .fst before a model of the same name), and its picture is an image file of the same name, if any.
 *
 * Created by gcalero on 1/21/19
 */
public class AvatarProvider {

    private static final String TAG = "Interface";

    private static final String AVATARS_JSON = "avatars.json";
    private static final String JSON_FIELD_NAME = "name";
    private static final String JSON_FIELD_URL = "url";
    private static final String JSON_FIELD_IMAGE = "preview_image";
    private static final String JSON_FIELD_AVATARS_ARRAY = "avatars";

    private static final String FILES_API_URL = HifiUtils.METAVERSE_BASE_URL + "/api/user/files";
    private static final String AVATARS_FOLDER = "avatars";
    // in order of preference, for files of the same name
    private static final String[] MODEL_EXTENSIONS = { "fst", "glb", "vrm", "gltf", "fbx" };
    private static final String[] IMAGE_EXTENSIONS = { "jpg", "jpeg", "png", "webp" };
    private static final int TIMEOUT_MSECS = 15000;

    private final Context mContext;
    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    public interface AvatarsCallback {
        void retrieveOk(List<AvatarAdapter.Avatar> avatars);
        void retrieveError(Exception e, String message);
    }

    public AvatarProvider(Context context) {
        mContext = context;
    }

    public void retrieve(AvatarsCallback avatarsCallback) {
        mExecutor.execute(() -> {
            List<AvatarAdapter.Avatar> avatars = new ArrayList<>();
            try {
                avatars.addAll(retrieveFromFiles());
            } catch (IOException | JSONException e) {
                // the built-in avatars are still listed
                Log.w(TAG, "Could not list the avatars in your files", e);
            }
            try {
                avatars.addAll(retrieveFromAssets());
            } catch (IOException | JSONException e) {
                mMainHandler.post(() -> avatarsCallback.retrieveError(e, "Failed reading avatars.json"));
                return;
            }
            mMainHandler.post(() -> avatarsCallback.retrieveOk(avatars));
        });
    }

    private List<AvatarAdapter.Avatar> retrieveFromFiles() throws IOException, JSONException {
        List<AvatarAdapter.Avatar> avatars = new ArrayList<>();
        String token = HifiUtils.getInstance().getAccessToken();
        if (token == null || token.isEmpty()) {
            return avatars;
        }

        HttpURLConnection connection = (HttpURLConnection) new URL(FILES_API_URL).openConnection();
        connection.setConnectTimeout(TIMEOUT_MSECS);
        connection.setReadTimeout(TIMEOUT_MSECS);
        connection.setRequestProperty("Authorization", "Bearer " + token);
        connection.setRequestProperty("Accept", "application/json");
        try {
            int status = connection.getResponseCode();
            if (status != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + status + " from " + FILES_API_URL);
            }
            JSONObject response = new JSONObject(readAll(connection.getInputStream()));
            String baseUrl = response.getString("url");
            JSONArray files = response.getJSONArray("files");

            // the avatars folder's files, by path without extension, then extension
            Map<String, Set<String>> extensionsByBase = new HashMap<>();
            Map<String, String> keysByBaseAndExtension = new HashMap<>();
            for (int i = 0; i < files.length(); i++) {
                String key = files.getJSONObject(i).getString("key");
                // keys start with a slash: "/avatars/name.fst"
                String path = key.startsWith("/") ? key.substring(1) : key;
                int slash = path.indexOf('/');
                int dot = path.lastIndexOf('.');
                if (slash <= 0 || dot <= slash || !path.substring(0, slash).equalsIgnoreCase(AVATARS_FOLDER)) {
                    continue;
                }
                String base = path.substring(0, dot).toLowerCase(Locale.ROOT);
                String extension = path.substring(dot + 1).toLowerCase(Locale.ROOT);
                extensionsByBase.computeIfAbsent(base, b -> new HashSet<>()).add(extension);
                keysByBaseAndExtension.put(base + "." + extension, key);
            }

            List<String> bases = new ArrayList<>(extensionsByBase.keySet());
            Collections.sort(bases);
            for (String base : bases) {
                Set<String> extensions = extensionsByBase.get(base);
                String modelKey = firstKey(base, extensions, MODEL_EXTENSIONS, keysByBaseAndExtension);
                if (modelKey == null) {
                    continue;
                }
                String imageKey = firstKey(base, extensions, IMAGE_EXTENSIONS, keysByBaseAndExtension);

                AvatarAdapter.Avatar avatar = new AvatarAdapter.Avatar();
                String fileName = modelKey.substring(modelKey.lastIndexOf('/') + 1);
                avatar.avatarName = fileName.substring(0, fileName.lastIndexOf('.'));
                avatar.avatarUrl = baseUrl + Uri.encode(modelKey, "/");
                avatar.avatarPreviewUrl = imageKey != null ? baseUrl + Uri.encode(imageKey, "/") : null;
                avatars.add(avatar);
            }
            Log.d(TAG, "Avatars in your files: " + avatars.size() + " of " + files.length() + " files");
            if (avatars.isEmpty()) {
                Set<String> folders = new HashSet<>();
                for (int i = 0; i < files.length(); i++) {
                    String path = files.getJSONObject(i).getString("key").replaceFirst("^/", "");
                    folders.add(path.contains("/") ? path.substring(0, path.indexOf('/')) + "/" : "(top level)");
                }
                Log.d(TAG, "No avatars folder with models; your files' folders: " + folders);
            }
        } finally {
            connection.disconnect();
        }
        return avatars;
    }

    private static String firstKey(String base, Set<String> extensions, String[] preferred,
                                   Map<String, String> keysByBaseAndExtension) {
        for (String extension : preferred) {
            if (extensions.contains(extension)) {
                return keysByBaseAndExtension.get(base + "." + extension);
            }
        }
        return null;
    }

    private List<AvatarAdapter.Avatar> retrieveFromAssets() throws IOException, JSONException {
        JSONObject obj = new JSONObject(readAll(mContext.getAssets().open(AVATARS_JSON)));
        JSONArray m_jArry = obj.getJSONArray(JSON_FIELD_AVATARS_ARRAY);
        ArrayList<AvatarAdapter.Avatar> avatars = new ArrayList<>();

        for (int i = 0; i < m_jArry.length(); i++) {
            JSONObject jo_inside = m_jArry.getJSONObject(i);
            AvatarAdapter.Avatar anAvatar = new AvatarAdapter.Avatar();
            anAvatar.avatarName = jo_inside.getString(JSON_FIELD_NAME);
            anAvatar.avatarPreviewUrl = jo_inside.optString(JSON_FIELD_IMAGE, null);
            anAvatar.avatarUrl = jo_inside.getString(JSON_FIELD_URL);
            avatars.add(anAvatar);
        }
        return avatars;
    }

    private static String readAll(InputStream is) throws IOException {
        try (InputStream in = is) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            return out.toString("UTF-8");
        }
    }
}
