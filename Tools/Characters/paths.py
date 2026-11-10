"""Shared repository-relative paths for the character asset pipeline."""
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
ASSETS = REPOSITORY / "Assets"
ART_SOURCE = REPOSITORY / "ArtSource"

def repository_path(value):
    path = Path(value).expanduser()
    return path if path.is_absolute() else REPOSITORY / path

def source_workspace(directory):
    """Sidecar metadata/parts for shipped characters; custom outputs stay self-contained."""
    directory = Path(directory).resolve()
    players = (ASSETS / 'Models/Player').resolve()
    if directory.parent == players:
        result = ART_SOURCE / 'Characters' / directory.name / 'Derived'
        result.mkdir(parents=True, exist_ok=True)
        return result
    return directory
