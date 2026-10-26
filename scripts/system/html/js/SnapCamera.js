//
//  SnapCamera.js
//  scripts/system/html/js
//
//  Snap's camera on desktop: one part of it per window (?part=frame|bar|dock). Shows the state snapshot.js sends
//  and sends back what the user does; it decides nothing itself.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

/* global EventBridge, openEventBridge */

(function () {
    "use strict";

    var PART = (/[?&]part=(\w+)/.exec(location.search) || [])[1] || "frame";
    var RING_LENGTH = 282.7; // the shutter ring's circumference (r = 45)
    var CORNER_INSET = 14;
    var state = null;
    var shownCountdown = 0;
    var deleteArmedUntil = 0;

    function byId(id) {
        return document.getElementById(id);
    }

    function send(action, extra) {
        if (!EventBridge) {
            return;
        }
        var message = { type: "snapCamera", part: PART, action: action };
        Object.keys(extra || {}).forEach(function (key) {
            message[key] = extra[key];
        });
        EventBridge.emitWebEvent(JSON.stringify(message));
    }

    // A picture's location as an image source: a URL as it is, a path on this computer as a file URL.
    function imageSource(location) {
        if (!location) {
            return "";
        }
        if (/^(https?|file|qrc):/i.test(location)) {
            return location;
        }
        if (/^[A-Za-z]:[\\/]/.test(location)) {
            return "file:///" + location.replace(/\\/g, "/");
        }
        return "file://" + location;
    }

    function place(element, x, y, w, h) {
        element.style.left = x + "px";
        element.style.top = y + "px";
        element.style.width = Math.max(0, w) + "px";
        element.style.height = Math.max(0, h) + "px";
    }

    function setImage(img, location) {
        var source = imageSource(location);
        if (img.getAttribute("src") !== source) {
            img.setAttribute("src", source);
        }
    }

    // --- the frame over the world ---------------------------------------------------------------------------------

    function renderFrame() {
        var crop = state.crop;
        var width = state.width;
        var height = state.height;
        place(byId("band-top"), 0, 0, width, crop.y);
        place(byId("band-bottom"), 0, crop.y + crop.h, width, height - crop.y - crop.h);
        place(byId("band-left"), 0, crop.y, crop.x, crop.h);
        place(byId("band-right"), crop.x + crop.w, crop.y, width - crop.x - crop.w, crop.h);
        var size = 26;
        place(byId("corner-tl"), crop.x + CORNER_INSET, crop.y + CORNER_INSET, size, size);
        place(byId("corner-tr"), crop.x + crop.w - CORNER_INSET - size, crop.y + CORNER_INSET, size, size);
        place(byId("corner-bl"), crop.x + CORNER_INSET, crop.y + crop.h - CORNER_INSET - size, size, size);
        place(byId("corner-br"), crop.x + crop.w - CORNER_INSET - size, crop.y + crop.h - CORNER_INSET - size, size, size);
        place(byId("flash"), crop.x, crop.y, crop.w, crop.h);
        place(byId("countdown"), crop.x, crop.y, crop.w, crop.h);

        var counting = state.counting && state.countdown > 0;
        byId("countdown").hidden = !counting;
        if (counting && state.countdown !== shownCountdown) {
            var number = byId("countdown-number");
            number.textContent = String(state.countdown);
            restartAnimation(number, "pop");
            restartAnimation(byId("countdown-rabbit"), "hop");
        }
        shownCountdown = counting ? state.countdown : 0;

        var toast = state.toast;
        byId("toast").hidden = !toast;
        if (toast) {
            byId("toast").className = "glass " + toast.kind;
            byId("toast-icon").setAttribute("stroke", toast.kind === "ok" ? "#00afff" : "#9aa1bd");
            byId("toast-text").textContent = toast.text;
            byId("toast-detail").textContent = toast.detail || "";
        }

        renderReview();
    }

    function restartAnimation(element, name) {
        element.classList.remove(name);
        void element.offsetWidth; // a reflow, so the animation starts again
        element.classList.add(name);
    }

    function flash() {
        restartAnimation(byId("flash"), "go");
    }

    function renderReview() {
        var review = state.review;
        var open = !!(review && review.open && review.items.length > 0);
        byId("review").hidden = !open;
        if (!open) {
            return;
        }
        var item = review.items[review.selected] || review.items[0];
        byId("review-when").textContent = item.when || "";
        var image = byId("review-image");
        if (!image.matches(":hover") || !item.gifSrc) {
            setImage(image, item.src);
        }
        byId("review-gif").hidden = !item.gifSrc;
        byId("review-copy").hidden = !item.inUserFiles;
        byId("review-files").hidden = !item.inUserFiles;
        byId("review-delete").hidden = !item.canDelete;
        byId("review-where").textContent = item.where || "";

        var strip = byId("review-strip");
        strip.hidden = review.items.length < 2;
        while (strip.firstChild) {
            strip.removeChild(strip.firstChild);
        }
        review.items.forEach(function (each, index) {
            var button = document.createElement("button");
            button.type = "button";
            button.setAttribute("role", "listitem");
            button.setAttribute("aria-label", index === 0 ? "Your newest Snap" : "An earlier Snap");
            if (index === review.selected) {
                button.className = "selected";
            }
            var thumb = document.createElement("img");
            thumb.alt = "";
            setImage(thumb, each.src);
            button.appendChild(thumb);
            button.addEventListener("click", function () {
                send("select", { index: index });
            });
            strip.appendChild(button);
        });
    }

    function wireReview() {
        byId("review-close").addEventListener("click", function () {
            send("closeReview");
        });
        byId("review").addEventListener("click", function (event) {
            if (event.target === byId("review")) {
                send("closeReview");
            }
        });
        var picture = byId("review-picture");
        picture.addEventListener("mouseenter", function () {
            var item = state && state.review && state.review.items[state.review.selected];
            if (item && item.gifSrc) {
                setImage(byId("review-image"), item.gifSrc);
            }
        });
        picture.addEventListener("mouseleave", function () {
            var item = state && state.review && state.review.items[state.review.selected];
            if (item) {
                setImage(byId("review-image"), item.src);
            }
        });
        byId("review-copy").addEventListener("click", function () {
            send("copyLink");
            byId("review-copy-label").textContent = "Copied";
            setTimeout(function () {
                byId("review-copy-label").textContent = "Copy link";
            }, 1500);
        });
        byId("review-files").addEventListener("click", function () {
            send("showInMyFiles");
        });
        byId("review-delete").addEventListener("click", function () {
            // the first click asks, a second within three seconds deletes
            if (Date.now() < deleteArmedUntil) {
                deleteArmedUntil = 0;
                byId("review-delete-label").textContent = "Delete";
                send("delete");
                return;
            }
            deleteArmedUntil = Date.now() + 3000;
            byId("review-delete-label").textContent = "Delete it for good?";
            setTimeout(function () {
                if (Date.now() >= deleteArmedUntil) {
                    byId("review-delete-label").textContent = "Delete";
                }
            }, 3100);
        });
    }

    // --- the switch bar -------------------------------------------------------------------------------------------

    function renderBar() {
        setOn(byId("gif"), state.gif);
        setOn(byId("nametags"), state.nametags);
        setOn(byId("timer"), state.timer > 0);
        byId("timer-label").textContent = state.timer > 0 ? state.timer + "s" : "Off";
    }

    function setOn(button, on) {
        button.classList.toggle("on", !!on);
        button.setAttribute("aria-pressed", on ? "true" : "false");
    }

    function wireBar() {
        byId("close").addEventListener("click", function () {
            send("close");
        });
        byId("gif").addEventListener("click", function () {
            send("toggleGif");
        });
        byId("nametags").addEventListener("click", function () {
            send("toggleNametags");
        });
        byId("timer").addEventListener("click", function () {
            send("cycleTimer");
        });
        byId("settings").addEventListener("click", function () {
            send("settings");
        });
    }

    // --- the shutter bar ------------------------------------------------------------------------------------------

    function renderDock() {
        var last = state.last;
        byId("thumb-image").hidden = !last;
        byId("thumb-empty").hidden = !!last;
        byId("thumb-badge").hidden = !last;
        if (last) {
            setImage(byId("thumb-image"), last.src);
            byId("thumb-badge").className = "badge " + last.badge;
        }
        byId("thumb").disabled = !last;

        var shutter = byId("shutter");
        shutter.disabled = !!state.busy && !state.counting;
        shutter.classList.toggle("counting", !!state.counting);
        shutter.setAttribute("aria-label", state.counting ? "Stop the self-timer" : "Take a Snap");
        var ring = byId("ring-progress");
        var progress = typeof state.progress === "number" ? Math.max(0, Math.min(1, state.progress)) : 0;
        ring.setAttribute("stroke", state.progressColor === "violet" ? "#a548ff" : "#00afff");
        ring.setAttribute("stroke-dasharray", (progress * RING_LENGTH).toFixed(1) + " " + RING_LENGTH);

        setOn(byId("flip"), state.selfie);
    }

    function wireDock() {
        byId("thumb").addEventListener("click", function () {
            send("openReview");
        });
        byId("shutter").addEventListener("click", function () {
            send("shutter");
        });
        byId("flip").addEventListener("click", function () {
            send("flip");
        });
    }

    // --- all parts ------------------------------------------------------------------------------------------------

    function render() {
        if (!state) {
            return;
        }
        if (PART === "frame") {
            renderFrame();
        } else if (PART === "bar") {
            renderBar();
        } else if (PART === "dock") {
            renderDock();
        }
    }

    window.addEventListener("load", function () {
        var part = byId(PART + "-part");
        if (!part) {
            return;
        }
        part.hidden = false;
        if (PART === "frame") {
            wireReview();
        } else if (PART === "bar") {
            wireBar();
        } else if (PART === "dock") {
            wireDock();
        }
        document.addEventListener("keydown", function (event) {
            if (event.key === "Escape") {
                send("escape");
            }
        });
        openEventBridge(function () {
            EventBridge.scriptEventReceived.connect(function (text) {
                var message;
                try {
                    message = JSON.parse(text);
                } catch (error) {
                    return;
                }
                if (!message || message.type !== "snapCamera") {
                    return;
                }
                if (message.action === "state") {
                    state = message.state;
                    render();
                } else if (message.action === "flash" && PART === "frame") {
                    flash();
                }
            });
            send("ready");
        });
    });
}());
