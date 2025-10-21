# 🗺️ Development Roadmap: F8R Hybrid Sequencer/Controller

This roadmap details the progression from core data structure initialization to implementing the final dynamic sequencing and locking logic in the Disting NT Lua script.

---

## Phase 1: Core Database & Fader Naming (v0.1 – v0.55)

The goal of this phase is to establish the persistent **64-slot virtual fader database** and the controls needed to edit the names for your Custom UI.

| Version | Feature Goal | Implementation Details & Testing Focus |
| :--- | :--- | :--- |
| **v0.1** | **64-Fader Memory & Default Naming** | Implement and initialize the full **`self.FaderValues[64]`** (stores 0.0–1.0 fader position) and **`self.FaderNames[64]`** arrays. Initialize all names to the default format: **`FADR [01-64]`**. |
| **v0.2** | **Page Selection & Basic GUI Display** | Implement **`Page Flip CV`** (Input 1) logic. The custom UI must display the correct **Page Number** (1-8) and the **8 percentage values** for the current page. |
| **v0.3** | **Manual Paging (Encoder Navigation)** | Implement the **`encoder1Turn()`** function to allow the user to manually flip between **Page 1** and **Page 8** using the physical encoder knob. |
| **v0.52** | **Fader Index Parameter** | Add the parameter: **`VFDR Edit Index`** (Range 1–64). This selects which of the 64 fader slots is active for naming. |
| **v0.53** | **Fader Name String Parameter** | Add the **String Parameter** (`VFDR Name`). Implement the logic to instantly **read and write** the name of the fader selected by `VFDR Edit Index` into the `self.FaderNames` array. |
| **v0.55** | **Finalize Max Length** | **Testing Focus**: Manually test the longest string that is fully visible on the Disting NT GUI to set the definitive **Maximum String Length** (e.g., 12 characters). *The final code must enforce this limit.* |

---

## Phase 2: Dynamic Hybrid Logic & Non-Destructive Editing (v0.6 – v0.7)

This phase establishes the dynamic roles of the faders based on the sequence length and ensures that user-defined names are not lost.

| Version | Feature Goal | Implementation Details & Testing Focus |
| :--- | :--- | :--- |
| **v0.6** | **Sequencer Naming Overwrite** | Update the `draw()` function: If a virtual fader is actively a step ($\le$ **`Seq Length`**), the GUI must display the functional name: **`STEP [N]`** (e.g., `STEP 4`), preserving the custom name in memory. |
| **v0.7** | **Pitch/CC Mode Indicator** | Implement the UI logic to switch the fader's **value display**: display **Note Value** (e.g., `C3`) for reserved steps and display **Percentage Value** (e.g., `54%`) for faders that are currently free for CC modulation. |

---

## Phase 3: Sequencer Operational Logic & Debugging (v0.8 – v1.1)

This final phase adds the required timing, pitch output, locking, and the essential monitoring tools.

| Version | Feature Goal | Implementation Details & Testing Focus |
| :--- | :--- | :--- |
| **v0.8** | **Full Sequencer Core** | Implement the **Clock In** (Input 2), **Gate Out** (Output 2), **Pitch CV** (Output 1), and **Dynamic Loop** logic. The sequence must correctly advance and loop according to the length set by `Seq Length`. |
| **v0.9** | **Takeover Logic** | Implement and verify the **`self.FaderTakeover`** logic. When the page is flipped, the outputs must **lock** until the physical fader matches the required value within the 2% threshold. |
| **v1.0** | **Direction Control** | Implement the full **8-mode Sequence Direction Logic** (Forward, Reverse, Random, Evens/Odds, etc.) using the `Direction CV` (Input 3). |
| **v1.1** | **CC Value Debug Pages** | Add two new debug pages (**Page 9** and **Page 10**) that list the raw live value and CC index for all **CC #0 through CC #63** (32 per page). |