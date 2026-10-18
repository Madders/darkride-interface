package io.highfidelity.hifiinterface.theme;

import android.content.Context;
import android.content.res.AssetManager;
import android.net.Uri;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * Installing, removing, exporting and customising themes (files/themes/&lt;id&gt;). A theme is a zip of data only:
 * theme.json, fonts/*.ttf|otf, icons/*.png, licence and readme text files and a preview.png. Anything else, anything
 * outside the theme's folder, or anything too big is refused, and nothing in a theme runs.
 */
public final class ThemeInstaller {
    public static final String MY_THEME = "my_theme";

    private static final int MAX_FILES = 64;
    private static final long MAX_FILE_BYTES = 5L * 1024 * 1024;
    private static final long MAX_TOTAL_BYTES = 20L * 1024 * 1024;

    // Why a theme couldn't be installed, for the person installing it
    public static class ThemeException extends Exception {
        public ThemeException(String message) {
            super(message);
        }
    }

    private ThemeInstaller() {}

    // Installs (or replaces) the theme in a zip, returning its id
    public static String install(Context context, Uri zip) throws ThemeException {
        Map<String, byte[]> files = new HashMap<>();
        long total = 0;
        try (InputStream in = context.getContentResolver().openInputStream(zip);
             ZipInputStream zipIn = new ZipInputStream(in)) {
            ZipEntry entry;
            while ((entry = zipIn.getNextEntry()) != null) {
                if (entry.isDirectory()) {
                    continue;
                }
                if (files.size() >= MAX_FILES) {
                    throw new ThemeException("The theme has too many files");
                }
                byte[] data = readLimited(zipIn, MAX_FILE_BYTES, entry.getName());
                total += data.length;
                if (total > MAX_TOTAL_BYTES) {
                    throw new ThemeException("The theme is too big");
                }
                files.put(entry.getName().replace('\\', '/'), data);
            }
        } catch (IOException | SecurityException e) {
            throw new ThemeException("Couldn't read the theme: " + e.getMessage());
        }

        // the files may be in a folder in the zip
        String prefix = "";
        if (!files.containsKey("theme.json")) {
            for (String name : files.keySet()) {
                if (name.endsWith("/theme.json") && name.indexOf('/') == name.length() - "/theme.json".length()) {
                    prefix = name.substring(0, name.length() - "theme.json".length());
                    break;
                }
            }
        }
        byte[] json = files.get(prefix + "theme.json");
        if (json == null) {
            throw new ThemeException("That isn't a theme: it has no theme.json");
        }
        String id;
        try {
            JSONObject theme = new JSONObject(new String(json, StandardCharsets.UTF_8));
            id = theme.optString("id", "");
            theme.getJSONObject("colors");
        } catch (Exception e) {
            throw new ThemeException("The theme's theme.json can't be read: " + e.getMessage());
        }
        if (!id.matches(ThemeManager.ID_PATTERN)) {
            throw new ThemeException("The theme's id must be lower-case letters, digits and _");
        }
        if (ThemeManager.isBuiltIn(context, id)) {
            throw new ThemeException("The app already has a theme called " + id);
        }

        File staging = new File(context.getCacheDir(), "theme-install");
        deleteRecursively(staging);
        for (Map.Entry<String, byte[]> file : files.entrySet()) {
            if (!file.getKey().startsWith(prefix)) {
                continue;
            }
            String path = file.getKey().substring(prefix.length());
            if (!allowed(path)) {
                throw new ThemeException("Themes can't have " + path + " in them");
            }
            File target = new File(staging, path);
            target.getParentFile().mkdirs();
            try (OutputStream out = new FileOutputStream(target)) {
                out.write(file.getValue());
            } catch (IOException e) {
                throw new ThemeException("Couldn't save the theme: " + e.getMessage());
            }
        }
        try {
            Theme.load(staging);
        } catch (Exception e) {
            deleteRecursively(staging);
            throw new ThemeException("The theme doesn't load: " + e.getMessage());
        }
        File folder = new File(ThemeManager.installedFolder(context), id);
        deleteRecursively(folder);
        folder.getParentFile().mkdirs();
        if (!staging.renameTo(folder)) {
            deleteRecursively(staging);
            throw new ThemeException("Couldn't save the theme");
        }
        return id;
    }

    public static void remove(Context context, String id) {
        if (id.matches(ThemeManager.ID_PATTERN) && !ThemeManager.isBuiltIn(context, id)) {
            deleteRecursively(new File(ThemeManager.installedFolder(context), id));
        }
    }

    // Writes a theme (the app's own or installed) as a zip, to share
    public static void export(Context context, String id, Uri destination) throws IOException {
        try (OutputStream out = context.getContentResolver().openOutputStream(destination);
             ZipOutputStream zip = new ZipOutputStream(out)) {
            if (ThemeManager.isBuiltIn(context, id)) {
                zipAssets(context.getAssets(), ThemeManager.THEMES_FOLDER + "/" + id, "", zip);
            } else {
                zipFolder(new File(ThemeManager.installedFolder(context), id), "", zip);
            }
        }
    }

    // A changed copy of a theme, as "My theme": the base theme's fonts and icons with the changes made to its theme.json
    public static void saveCustom(Context context, String baseId, JSONObject changes) throws IOException {
        File staging = new File(context.getCacheDir(), "theme-custom");
        deleteRecursively(staging);
        staging.mkdirs();
        JSONObject json;
        try {
            if (ThemeManager.isBuiltIn(context, baseId)) {
                copyAssets(context.getAssets(), ThemeManager.THEMES_FOLDER + "/" + baseId, staging);
            } else {
                copyFolder(new File(ThemeManager.installedFolder(context), baseId), staging);
            }
            json = new JSONObject(ThemeManager.readFile(new File(staging, "theme.json")));
            merge(json, changes);
            json.put("id", MY_THEME);
            json.put("name", "My theme");
        } catch (Exception e) {
            deleteRecursively(staging);
            throw new IOException("Couldn't make the theme: " + e.getMessage(), e);
        }
        try (OutputStream out = new FileOutputStream(new File(staging, "theme.json"))) {
            out.write(json.toString(4).getBytes(StandardCharsets.UTF_8));
        } catch (Exception e) {
            throw new IOException("Couldn't save the theme", e);
        }
        File folder = new File(ThemeManager.installedFolder(context), MY_THEME);
        deleteRecursively(folder);
        folder.getParentFile().mkdirs();
        if (!staging.renameTo(folder)) {
            throw new IOException("Couldn't save the theme");
        }
    }

    private static boolean allowed(String path) {
        String lower = path.toLowerCase(Locale.ROOT);
        if (lower.contains("..") || lower.startsWith("/") || lower.contains(":")) {
            return false;
        }
        String[] parts = lower.split("/");
        if (parts.length == 1) {
            return lower.equals("theme.json") || lower.equals("preview.png") || lower.endsWith(".txt") || lower.endsWith(".md");
        }
        if (parts.length == 2 && parts[0].equals("fonts")) {
            return lower.endsWith(".ttf") || lower.endsWith(".otf") || lower.endsWith(".txt");
        }
        if (parts.length == 2 && parts[0].equals("icons")) {
            return lower.endsWith(".png");
        }
        return false;
    }

    private static void merge(JSONObject into, JSONObject changes) throws org.json.JSONException {
        java.util.Iterator<String> keys = changes.keys();
        while (keys.hasNext()) {
            String key = keys.next();
            Object value = changes.get(key);
            if (value instanceof JSONObject && into.optJSONObject(key) != null) {
                merge(into.getJSONObject(key), (JSONObject) value);
            } else {
                into.put(key, value);
            }
        }
    }

    private static byte[] readLimited(InputStream in, long limit, String name) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        long total = 0;
        while ((count = in.read(buffer)) > 0) {
            total += count;
            if (total > limit) {
                throw new IOException(name + " is too big");
            }
            out.write(buffer, 0, count);
        }
        return out.toByteArray();
    }

    private static void zipAssets(AssetManager assets, String path, String zipPath, ZipOutputStream zip) throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            zip.putNextEntry(new ZipEntry(zipPath));
            try (InputStream in = assets.open(path)) {
                copy(in, zip);
            }
            zip.closeEntry();
            return;
        }
        for (String child : children) {
            zipAssets(assets, path + "/" + child, zipPath.isEmpty() ? child : zipPath + "/" + child, zip);
        }
    }

    private static void zipFolder(File folder, String zipPath, ZipOutputStream zip) throws IOException {
        File[] children = folder.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            String childPath = zipPath.isEmpty() ? child.getName() : zipPath + "/" + child.getName();
            if (child.isDirectory()) {
                zipFolder(child, childPath, zip);
            } else {
                zip.putNextEntry(new ZipEntry(childPath));
                try (InputStream in = new FileInputStream(child)) {
                    copy(in, zip);
                }
                zip.closeEntry();
            }
        }
    }

    private static void copyAssets(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            target.getParentFile().mkdirs();
            try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
                copy(in, out);
            }
            return;
        }
        target.mkdirs();
        for (String child : children) {
            copyAssets(assets, path + "/" + child, new File(target, child));
        }
    }

    private static void copyFolder(File source, File target) throws IOException {
        File[] children = source.listFiles();
        target.mkdirs();
        if (children == null) {
            return;
        }
        for (File child : children) {
            if (child.isDirectory()) {
                copyFolder(child, new File(target, child.getName()));
            } else {
                try (InputStream in = new FileInputStream(child);
                     OutputStream out = new FileOutputStream(new File(target, child.getName()))) {
                    copy(in, out);
                }
            }
        }
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[8192];
        int count;
        while ((count = in.read(buffer)) > 0) {
            out.write(buffer, 0, count);
        }
    }

    static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        file.delete();
    }
}
