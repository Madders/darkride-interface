declare namespace Friends {
	// properties
	const availability: "all" | "friends" | "none" | "";
	const canMessage: boolean;
	const requestCount: number;

	// methods
	function openDirectMessage(username: string): void;
	function setAvailability(value: "all" | "friends" | "none"): void;
	function refresh(): void;

	// signals
	const availabilityChanged: Signal<() => any>;
	const canMessageChanged: Signal<() => any>;
	const requestCountChanged: Signal<() => any>;
}
