// Prints every function Ghidra has marked non-returning.
//@category Xbox
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class ListNoReturn extends GhidraScript {
	@Override
	protected void run() throws Exception {
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			if (f.hasNoReturn()) {
				println("NORETURN " + f.getEntryPoint() + " " + f.getName(true));
			}
		}
	}
}
