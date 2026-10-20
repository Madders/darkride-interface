import { ButtonData } from "../button-manager";
import { WebEventHandler } from "../web-event-handler";

export class ExploreHandler extends WebEventHandler {
	canGoBack = false;
	canGoForward = false;

	// the system sounds: a soft blip when the pointer comes onto a world or a friend, a rising pop as you hop
	// (both are stereo, so they play as the tablet's own sounds do, not placed in the world)
	private hoverSound = SoundCache.getSound("qrc:/sounds/ButtonHover.wav");
	private hopSound = SoundCache.getSound("qrc:/sounds/TabletOpen.wav");
	private lastHover = 0;
	// a hop's pop plays at the click; the join that follows (after an instance has started) stays quiet
	private hopping = false;

	constructor(uuid: string, button: ButtonData, otherButtons: ButtonData[]) {
		super(uuid, button, otherButtons);

		this.signalManager.connect(
			Window.location.goBackPossible,
			(canGoBack: boolean) => {
				if (canGoBack != this.canGoBack) {
					this.canGoBack = canGoBack;
					this.emitEvent("canGoBack", this.canGoBack);
				}
			},
		);

		this.signalManager.connect(
			Window.location.goForwardPossible,
			(canGoForward: boolean) => {
				if (canGoForward != this.canGoForward) {
					this.canGoForward = canGoForward;
					this.emitEvent("canGoForward", this.canGoForward);
				}
			},
		);

		this.signalManager.connect(Window.location.hostChanged, () => {
			this.emitEvent(
				"getCurrentDomainId",
				this.uuidToId(Window.location.domainID),
			);
		});

		// hostChanged fires before the new domain's ID is known, and not at all between
		// domains on the same host (world instances differ only by port), so also report
		// the ID once the connection to a domain has completed
		this.signalManager.connect(Window.domainChanged, () => {
			this.emitEvent(
				"getCurrentDomainId",
				this.uuidToId(Window.location.domainID),
			);
		});
	}

	private uuidToId(uuid: string) {
		return uuid.replace("{", "").replace("}", "");
	}

	private play(sound: SoundObject) {
		if (sound.downloaded) Audio.playSound(sound, { localOnly: true });
	}

	handleEvent(data: { key: string; value: any }) {
		switch (data.key) {
			case "hover":
				// one blip per world or friend the pointer comes onto, not a burst as it sweeps across the list
				if (Date.now() - this.lastHover > 80) {
					this.lastHover = Date.now();
					this.play(this.hoverSound);
				}
				break;
			case "hopStart":
				// the loading screen takes over at once (worldLoading.js; harmless with no listener)
				this.hopping = true;
				this.play(this.hopSound);
				Messages.sendLocalMessage(
					"Darkride-Hop",
					JSON.stringify({
						type: "start",
						world: data.value.world,
						author: data.value.author,
						picture: data.value.picture,
					}),
				);
				break;
			case "hopCancel":
				// the hop failed before the client left: lift the screen, and bring Explore back to say why
				this.hopping = false;
				Messages.sendLocalMessage(
					"Darkride-Hop",
					JSON.stringify({ type: "cancel" }),
				);
				this.button.open();
				break;
			case "joinDomain":
				if (!this.hopping) this.play(this.hopSound);
				this.hopping = false;
				Window.location.handleLookupString(data.value);
				break;
			case "reconnect":
				this.play(this.hopSound);
				Window.location.reconnect();
				break;
			case "joinUser":
				if (!this.hopping) this.play(this.hopSound);
				this.hopping = false;
				Window.location.goToUser(data.value);
				break;

			case "canGoBack":
				this.emitEvent("canGoBack", this.canGoBack);
				break;
			case "canGoForward":
				this.emitEvent("canGoForward", this.canGoForward);
				break;

			case "goBack":
				AddressManager.goBack();
				break;
			case "goForward":
				AddressManager.goForward();
				break;

			case "getCurrentDomainId":
				this.emitEvent(
					"getCurrentDomainId",
					this.uuidToId(Window.location.domainID),
				);
				break;
			case "getCurrentDomainUserCount":
				this.emitEvent(
					"getCurrentDomainUserCount",
					AvatarManager.getAvatarIdentifiers().length,
				);
		}
	}
}
