// Dekompiliert Funktionen zu RVAs (Hex, eine pro Zeile). Argumente: Eingabedatei, Ausgabedatei
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.nio.file.*;

public class Decomp extends GhidraScript {
  @Override public void run() throws Exception {
    String[] a = getScriptArgs();
    DecompInterface d = new DecompInterface();
    d.openProgram(currentProgram);
    Address base = currentProgram.getImageBase();
    try (PrintWriter w = new PrintWriter(new FileWriter(a[1]))) {
      for (String l : Files.readAllLines(Paths.get(a[0]))) {
        l = l.trim();
        if (l.isEmpty()) continue;
        Address s = base.add(Long.parseLong(l, 16));
        Function f = getFunctionContaining(s);
        if (f == null) { w.println("// keine Funktion " + l); continue; }
        DecompileResults r = d.decompileFunction(f, 120, monitor);
        w.println("// ==== " + l);
        w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// Fehler " + r.getErrorMessage());
        w.flush();
      }
    }
  }
}
