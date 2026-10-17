from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR")).resolve().as_posix()
user = Path.home().resolve().as_posix()
env.Append(CCFLAGS=[f"-ffile-prefix-map={root}=Dash-Chan", f"-fmacro-prefix-map={root}=Dash-Chan", f"-ffile-prefix-map={user}=/builder", f"-fmacro-prefix-map={user}=/builder"])
