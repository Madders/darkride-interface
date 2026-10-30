import { HttpClientModule } from "@angular/common/http";
import { NgModule } from "@angular/core";
import { BrowserModule } from "@angular/platform-browser";
import { BrowserAnimationsModule } from "@angular/platform-browser/animations";
import { RouterModule, Routes } from "@angular/router";
import { AppComponent } from "./app.component";

const routes: Routes = [
	// {
	// 	path: "market",
	// 	loadChildren: () =>
	// 		import("./market/market.module").then(m => m.MarketModule),
	// },
	{
		path: "things",
		loadChildren: () =>
			import("./things/things.module").then(m => m.ThingsModule),
	},
	{
		path: "overview",
		loadChildren: () =>
			import("./overview/overview.module").then(m => m.OverviewModule),
	},
];

@NgModule({
	declarations: [AppComponent],
	imports: [
		BrowserModule,
		BrowserAnimationsModule,
		RouterModule.forRoot(routes, {
			useHash: true,
			scrollPositionRestoration: "enabled",
		}),
		HttpClientModule,
	],
	providers: [],
	bootstrap: [AppComponent],
})
export class AppModule {}
