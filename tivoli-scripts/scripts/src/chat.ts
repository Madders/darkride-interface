import { ButtonData } from "./buttons/button-manager";
import { SignalManager } from "./lib/signal-manager";
import { WebEventHandler } from "./buttons/web-event-handler";
import { usersCache } from "./users-cache";

class ChatJoinAndLeave {
	private signals = new SignalManager();

	private usernames: { [uuid: string]: string } = {};

	private getUsername = (
		requestId: Uuid,
		callback: (username: string) => any = () => {},
	) => {
		// const signals = new SignalManager();
		// signals.connect(
		// 	Users.usernameFromIDReply,
		// 	(id, username, machineFingerprint, isAdmin) => {
		// 		if (requestId != id) return;

		// 		const avatar = AvatarList.getAvatar(id);
		// 		if (avatar.displayName.toLowerCase() == username) {
		// 			this.usernames[id] = avatar.displayName;
		// 		} else {
		// 			this.usernames[id] = username; // lowercase
		// 		}
		// 		if (callback) callback(this.usernames[id]);

		// 		signals.cleanup();
		// 	},
		// );

		let tries = 50; // 5 seconds

		const interval = Script.setInterval(() => {
			if (tries <= 0) {
				Script.clearInterval(interval);
				return callback(null);
			}

			const avatar = AvatarList.getAvatar(requestId);
			const displayName = avatar ? avatar.displayName : null;

			if (
				displayName != null &&
				displayName != MyAvatar.displayName &&
				displayName != ""
			) {
				Script.clearInterval(interval);
				this.usernames[requestId] = displayName;
				return callback(displayName);
			}

			tries--;
		}, 100);
	};

	private isMovingBetweenWorlds = false;
	private isMovingBetweenWorldsInterval = null;

	private movingBetweenWorlds = () => {
		Script.clearTimeout(this.isMovingBetweenWorldsInterval);
		this.isMovingBetweenWorlds = true;
		this.isMovingBetweenWorldsInterval = Script.setTimeout(() => {
			this.isMovingBetweenWorlds = false;

			for (const uuid of AvatarList.getAvatarIdentifiers()) {
				this.getUsername(uuid);
			}

			this.onNewWorld();
		}, 500);
	};

	onJoin = (username: string) => {};
	onLeave = (username: string) => {};
	onNewWorld = () => {};

	constructor() {
		for (const uuid of AvatarList.getAvatarIdentifiers()) {
			this.getUsername(uuid);
		}

		this.signals.connect(
			MyAvatar.sessionUUIDChanged,
			this.movingBetweenWorlds,
		);
		this.signals.connect(
			AddressManager.hostChanged,
			this.movingBetweenWorlds,
		);

		this.signals.connect(AvatarList.avatarAddedEvent, uuid => {
			if (this.isMovingBetweenWorlds) return;
			this.getUsername(uuid, username => {
				this.onJoin(username == null ? "Someone" : username);
			});
		});

		this.signals.connect(AvatarList.avatarRemovedEvent, uuid => {
			if (this.isMovingBetweenWorlds) return;
			const username = this.usernames[uuid];
			delete this.usernames[uuid];
			this.onLeave(username == null ? "Someone" : username);
		});
	}

	cleanup() {
		this.signals.cleanup();
	}
}

class ChatHandler extends WebEventHandler {
	readonly channel = "chat";

	private joinAndLeave = new ChatJoinAndLeave();

	private currentChatSound = 0;
	private chatSounds = [
		SoundCache.getSound(Script.resolvePath("assets/chat1.wav")),
		SoundCache.getSound(Script.resolvePath("assets/chat2.wav")),
	];

	private commands: ChatCommand[] = [];

	private playChatSound() {
		const sound = this.chatSounds[this.currentChatSound];

		if (sound.downloaded)
			Audio.playSound(sound, {
				position: MyAvatar.position,
				localOnly: true,
				volume: 0.05,
			});

		this.currentChatSound = this.currentChatSound == 0 ? 1 : 0;
	}

	constructor(uuid: string, button: ButtonData) {
		super(uuid, button);

		this.signalManager.connect(Chat.messageReceived, (data, senderID) => {
			if (data.local) {
				this.emitEvent("showMessage", data);
			} else {
				// the name is the account's (the engine forces the display name to the username);
				// a sender with no name at all is shown as the mixer names it, never dropped
				const user = AvatarList.getAvatar(senderID);
				const cached = usersCache.get(senderID);
				const username =
					(user && user.displayName) ||
					(user && user.sessionDisplayName) ||
					(cached && cached.username) ||
					"anonymous";
				// an AI resident's mark comes from the metaverse by session id, never from the
				// message itself; the sender cannot set these (AI-FIRST-CLIENT.md §5, §10.4)
				const mark =
					cached && cached.ai
						? { ai: true, aiWord: cached.aiWord, host: cached.host }
						: { ai: false, aiWord: undefined, host: undefined };
				this.emitEvent("sendMessage", { ...data, username, ...mark });
			}
		});

		this.signalManager.connect(Controller.keyPressEvent, (e: KeyEvent) => {
			if (e.text == "\r" || e.text == "/") {
				this.emitEvent("focus", e.text == "/" ? { command: true } : {});
				this.button.panel.window.setFocus(true);
				this.button.panel.window.setEnabled(true);
			} else if (e.text == "ESC") {
				this.emitEvent("unfocus");
				this.button.panel.window.setFocus(false);
				this.button.panel.window.setEnabled(false);
			}
		});

		this.joinAndLeave.onJoin = username => {
			this.emitEvent("join", username);
		};
		this.joinAndLeave.onLeave = username => {
			this.emitEvent("leave", username);
		};
		this.joinAndLeave.onNewWorld = () => {
			this.emitEvent("clear");
		};

		const clearCommand = Chat.addCommand(
			"clear",
			"clears the chat for yourself",
		);
		this.signalManager.connect(clearCommand.running, params => {
			this.emitEvent("clear");
			Chat.showMessage("You cleared the chat for yourself.");
		});
		this.commands.push(clearCommand);

		const tpCommand = Chat.addCommand(
			"tp",
			"teleports to someone in world",
		);
		this.signalManager.connect(tpCommand.running, params => {
			const username = params.split(" ").shift().trim().toLowerCase();
			if (username == "") {
				Chat.showMessage("No username specified!");
				return;
			}

			let foundAvatar: ScriptAvatar = null;
			for (const id of AvatarList.getAvatarIdentifiers()) {
				const avatar = AvatarList.getAvatar(id);
				if (avatar.displayName.toLowerCase().indexOf(username) > -1) {
					foundAvatar = avatar;
					break;
				}
			}

			if (foundAvatar == null) {
				Chat.showMessage("User not found!");
			} else {
				Chat.showMessage("Teleporing to " + foundAvatar.displayName);
				MyAvatar.goToLocation(
					foundAvatar.position,
					true,
					foundAvatar.orientation,
					true,
				);
			}
		});
		this.commands.push(tpCommand);
	}

	// A link in chat is previewed by every reader's Darkride, so only web pages out on the internet are: not
	// other kinds of address, and not this computer or its local network. (QtScript has no startsWith/endsWith.)
	isPreviewable(url: string) {
		const match = /^https?:\/\/(\[[^\]]+\]|[^\/:?#]+)/i.exec(url);
		if (match == null) return false;
		const host = match[1].toLowerCase();
		if (host.charAt(0) == "[") return false; // IPv6 addresses by number
		if (/(^|\.)localhost$/.test(host) || /\.local$/.test(host)) return false;
		const ipv4 = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/.exec(host);
		if (ipv4 != null) {
			const a = parseInt(ipv4[1], 10);
			const b = parseInt(ipv4[2], 10);
			if (a == 0 || a == 10 || a == 127 || a >= 224) return false;
			if (a == 169 && b == 254) return false;
			if (a == 172 && b >= 16 && b <= 31) return false;
			if (a == 192 && b == 168) return false;
			if (a == 100 && b >= 64 && b <= 127) return false;
		} else if (/^[\d.]+$/.test(host) || host.indexOf(".") == -1) {
			return false; // a number that isn't a plain IPv4 address, or a name on the local network
		}
		return true;
	}

	getMetaTags(url: string, callback: (headContent: string) => any) {
		if (!this.isPreviewable(url)) return callback(null);
		request<string>(url, (error, response, html) => {
			if (error) return callback(null);
			if (response.headers["content-type"].indexOf("text/html") == -1)
				return callback(null);

			let metaTags = [];

			const matches = html.match(
				/(?:<meta [^]+?>)|(?:<link [^]+?>)|(?:<title[^]*?>[^]+?<\/title>)/gi,
			);
			if (matches != null) metaTags = matches;

			return callback(metaTags.join("\n"));
		});
	}

	handleEvent(data: { key: string; value: any }) {
		switch (data.key) {
			case "message":
				Chat.sendMessage(data.value);
				(Chat.messageSent as any)(data.value);
				break;
			case "command":
				const command = Chat.getCommand(data.value.command);
				if (command == null) {
					Chat.showMessage(
						"Command not found: " + data.value.command,
					);
				} else {
					command.run(data.value.params);
				}
				break;
			case "unfocus":
				this.button.panel.window.setFocus(false);
				this.button.panel.window.setEnabled(false);
				break;
			case "sound":
				this.playChatSound();
				break;
			case "openUrl":
				Window.openUrl(data.value);
				break;
			case "getMetaTags":
				this.getMetaTags(data.value, metaTags => {
					if (metaTags == null) return;
					this.emitEvent("getMetaTags", {
						url: data.value,
						metaTags,
					});
				});
		}
	}

	cleanup() {
		this.joinAndLeave.cleanup();
		this.signalManager.cleanup();

		for (const command of this.commands) {
			Chat.removeCommand(command);
		}
	}
}

export class ChatUI {
	signals = new SignalManager();

	window: OverlayWebWindow;
	handler: ChatHandler;

	readonly width = 512;
	readonly height = 512;
	readonly offsetX = 48;
	readonly offsetY = 48;

	fixPosition() {
		this.window.setPosition(
			this.offsetX,
			Overlays.height() - this.height - this.offsetY,
		);
	}

	constructor() {
		this.window = new OverlayWebWindow({
			width: this.width,
			height: this.height,
			visible: false,
			frameless: true,
			enabled: false,
			source:
				Script.resolvePath("ui/index.html") +
				"#/chat?metaverseUrl=" +
				AccountServices.metaverseServerURL,
		});
		this.fixPosition();
		this.window.setVisible(true);

		this.signals.connect(Window.geometryChanged, () => {
			this.fixPosition();
		});

		this.handler = new ChatHandler("com.tivolicloud.defaultScripts.chat", {
			button: null,
			panel: {
				window: this.window,
				entity: null,
			},
			open: null,
			close: null,
		});
		this.signals.connect(
			this.window.webEventReceived,
			this.handler.webEventReceived,
		);
	}

	cleanup() {
		this.handler.cleanup();
		this.signals.cleanup();
	}
}
