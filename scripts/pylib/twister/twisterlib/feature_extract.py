#!/usr/bin/env python3

from __future__ import annotations
import yaml
import json
from copy import deepcopy
from pathlib import Path
from typing import Optional, Tuple
from typing import Union


class Feature():
    def __init__(self, name: str, module: str, count: Union[int, str]):
        self.name : str = name
        self.module: str = module
        self.count = count

    def count_resolve(self, kconf: dict):
        if isinstance(self.count, str) and self.count in kconf:
            self.count = kconf[self.count]

    def __eq__(self, other):
        if isinstance(other, str):
            return self.name == other
        if isinstance(other, Feature):
            return str(self) == str(other)

        super().__eq__(other)

    def __hash__(self):
        return hash(self.name)

    def __str__(self) -> str:
        return f"{self.name} ({self.count})"

    def __repr__(self) -> str:
        return json.dumps({"name": self.name, "module": self.module, "count": self.count})

    @staticmethod
    def from_repr(rep: str) -> Feature:
        rep_dict = json.loads(rep)
        return Feature(**rep_dict)


class FeatureDB():
    def __init__(self, config_file: Optional[Path] = None):
        if config_file is None:
            config_file = Path(__file__).parent.resolve() / "features.yaml"

        conf = yaml.safe_load(open(config_file))

        self.features: dict[str, Feature] = {}
        self.name_feature_mapping: dict[str, Feature] = {}
        for module_name, module in conf.items():
            for feature_name, feature in module["features"].items():
                count = 1 if not  "count" in feature else feature["count"]
                feature_obj = Feature(name=feature_name, module=module_name, count=count)
                self.features[feature["kconfig"]] = feature_obj
                self.name_feature_mapping[feature_name] = feature_obj

    def __contains__(self, other):
        return other in self.features

    def __getitem__(self, key):
        return self.features[key]

    def samples_load(self, folder: Path) -> list[LocalSample]:
        samples = []

        for config_file in folder.glob("**/.config"):
            name = Sample.sample_name_get(config_file)
            samples.append(LocalSample(name, config_file, self))

        return samples

    def feature_with_name_get(self, feature_name: str):
        try:
            return self.name_feature_mapping[feature_name]
        except KeyError as e:
            print(f"Feature {feature_name} not defined.")
            from pprint import pprint
            print("The following features are available:")
            pprint(list(self.name_feature_mapping.keys()))
            print()
            raise e


class Sample():
    @staticmethod
    def sample_name_get(kconfig_path: Path) -> str:
        return kconfig_path.parts[-4]

    def __init__(self, name: str, features: set[Feature], ram: int, nvm: int) -> None:
        self.name: str = name
        self.features: set[Feature] = features
        self._ram = ram
        self._nvm = nvm

    @property
    def ram(self) -> int:
        return self._ram

    @property
    def nvm(self) -> int:
        return self._nvm

    def __repr__(self) -> str:
        return self.name


class LocalSample(Sample):
    def __init__(self, name: str, kconfig_file: Path, db: FeatureDB) -> None:
        kconfig: dict[str, int] = self._config_load(kconfig_file)
        self.kconfig_file: Path = kconfig_file

        features: set[Feature] = set()
        for k in kconfig:
            if k in db:
                feat = deepcopy(db[k])
                feat.count_resolve(kconfig)
                features.add(feat)
        self._size: Optional[Tuple[int, int]] = None
        super().__init__(name=name, features=features, ram=0, nvm=0)

    @staticmethod
    def _config_load(kconfig_file: Path) -> dict:
        enabled_features = dict()

        with open(kconfig_file) as fil:
            for line in fil:
                line=line.strip()
                if len(line) == 0:
                    # skip empty line
                    continue

                if 'is not set' in line:
                    sl = line.split(" ")
                    kconfig=sl[0][1:]
                    count = 0
                    enabled_features[kconfig] = count
                    continue


                sl = line.split("=")

                count = 0
                if len(sl) == 1:
                    pass
                elif sl[1] == "y":
                    count = 1;
                else:
                    try:
                        count = int(sl[1])
                    except:
                        count = sl[1]

                if sl[0][0] == "#":
                    kconfig=sl[0][1:]
                    count = 0
                else:
                    kconfig = sl[0]

                if not "CONFIG_" in kconfig:
                    continue

                enabled_features[kconfig] = count
                #enabled_features[modules[kconfig]].append((features[kconfig], count))

        return dict(enabled_features)
