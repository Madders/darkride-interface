declare namespace Panels {
	// methods
	function names(): string[];
	function isOpen(name: string): boolean;
	function open(name: string): void;
	function close(name: string): void;
	function toggle(name: string): void;
	function showProfile(username: string): void;

	// signals
	const openChanged: Signal<(name: string, open: boolean) => any>;
}
