"""Hand-authored geography of LA PLACE in normalised (u, v) map coordinates (Spec section 2: u east, v south).

The outlines are coarse polygons measured by hand against the Japanese reference layout (only the geography is used:
where the continents, bays, capes, ranges and cities are relative to each other). They are deliberately simplified;
all small-scale coastline shape is generated at runtime (domain warp + fractal noise), so no drawn line work from the
reference art is carried into the game.

Deviations from the reference that the generator relies on (documented in Docs/LaPlace/World_Generation.md):
  * the Heaven Continent is separated from the Northern Territories by a ~350 m strait (the reference shows a
    190 m neck); it is a cliff-walled plateau all the way round, as the spec asks;
  * the southern Begaritt coast is pulled ~200 m north and the Demon east coast ~250 m west so that every coast keeps
    a sea margin inside the landscape rectangle;
  * the tail of the Central Continent is widened ~150 m around the King Dragon capital so the 220 m city fits.
"""

# ------------------------------------------------------------------------------------------ continent outlines
CONTINENTS = {
    "Central": [
        (0.4571, 0.1528), (0.4562, 0.1650), (0.4146, 0.2017), (0.3929, 0.2439), (0.3971, 0.2672), (0.3871, 0.2894),
        (0.3904, 0.3061), (0.3746, 0.3383), (0.3754, 0.3539), (0.3562, 0.3839), (0.3554, 0.4172), (0.3629, 0.4494),
        (0.3471, 0.5139), (0.3638, 0.5572), (0.3762, 0.5661), (0.3796, 0.5761), (0.3904, 0.5906), (0.3846, 0.6306),
        (0.3912, 0.6506), (0.3771, 0.6706), (0.3888, 0.6917), (0.3838, 0.7150), (0.4096, 0.7383), (0.4121, 0.7550),
        (0.4238, 0.7572), (0.4296, 0.7739), (0.4454, 0.7928), (0.4546, 0.7928), (0.4679, 0.8060), (0.4946, 0.8180),
        (0.5062, 0.8150), (0.5171, 0.8300), (0.5262, 0.8328), (0.5362, 0.8517), (0.5204, 0.8639), (0.5038, 0.8610),
        (0.4921, 0.8660), (0.4762, 0.8530), (0.4588, 0.8494), (0.4521, 0.8417), (0.4421, 0.8539), (0.4321, 0.8294),
        (0.4162, 0.8283), (0.3846, 0.8106), (0.3488, 0.7706), (0.3304, 0.7650), (0.3304, 0.7472), (0.3079, 0.7228),
        (0.2954, 0.6939), (0.2462, 0.6661), (0.2404, 0.6539), (0.2254, 0.6550), (0.2188, 0.6228), (0.2037, 0.6150),
        (0.2004, 0.5961), (0.1612, 0.5694), (0.1512, 0.5361), (0.1362, 0.5094), (0.1112, 0.4261), (0.0988, 0.4150),
        (0.0954, 0.3906), (0.0788, 0.3828), (0.0688, 0.3661), (0.0562, 0.3683), (0.0496, 0.3517), (0.0404, 0.2650),
        (0.0338, 0.2528), (0.0488, 0.1817), (0.0271, 0.1239), (0.0190, 0.1170), (0.0150, 0.1000), (0.0150, 0.0800),
        (0.0210, 0.0660), (0.0300, 0.0760), (0.0571, 0.0728), (0.0662, 0.0639), (0.0912, 0.0639), (0.1038, 0.0717),
        (0.1488, 0.0650), (0.1579, 0.0639), (0.1762, 0.0639), (0.1950, 0.0620), (0.2130, 0.0830), (0.2321, 0.1061),
        (0.2454, 0.1094), (0.2488, 0.1194), (0.2679, 0.1217), (0.2912, 0.1472), (0.3079, 0.1406), (0.3196, 0.1583),
        (0.3321, 0.1528), (0.3529, 0.1672), (0.3579, 0.1561), (0.3704, 0.1639), (0.3804, 0.1539), (0.3988, 0.1594),
        (0.4521, 0.1439), (0.4612, 0.1272), (0.4680, 0.1300),
    ],
    "Heaven": [
        (0.4870, 0.1110), (0.4971, 0.0761), (0.5188, 0.0528), (0.5612, 0.0306), (0.5938, 0.0294), (0.6062, 0.0361),
        (0.6254, 0.0260), (0.6446, 0.0306), (0.6430, 0.0472), (0.6596, 0.0783), (0.6600, 0.1060), (0.6540, 0.1130),
        (0.6462, 0.1120), (0.6379, 0.1060), (0.6221, 0.1100), (0.5996, 0.1010), (0.5938, 0.0920), (0.5863, 0.0990),
        (0.5604, 0.0880), (0.5371, 0.0990), (0.5262, 0.0930), (0.5060, 0.1120), (0.4960, 0.1220), (0.4900, 0.1220),
    ],
    "Demon": [
        (0.6780, 0.1180), (0.6896, 0.1206), (0.7146, 0.0983), (0.7338, 0.1006), (0.7412, 0.0928), (0.7504, 0.1028),
        (0.7621, 0.0961), (0.7712, 0.1061), (0.7938, 0.1094), (0.8054, 0.1094), (0.8154, 0.0983), (0.8271, 0.0961),
        (0.8388, 0.1017), (0.8671, 0.1372), (0.8871, 0.1450), (0.9112, 0.1439), (0.9329, 0.1261), (0.9662, 0.1283),
        (0.9688, 0.1561), (0.9529, 0.1772), (0.9562, 0.1883), (0.9496, 0.2106), (0.9588, 0.2283), (0.9579, 0.2394),
        (0.9746, 0.2528), (0.9679, 0.2806), (0.9704, 0.3061), (0.9796, 0.3206), (0.9830, 0.3806), (0.9860, 0.4206),
        (0.9850, 0.4480), (0.9790, 0.4617), (0.9740, 0.4660), (0.9712, 0.4772), (0.9662, 0.5061), (0.9696, 0.5283),
        (0.9562, 0.5372), (0.9562, 0.5483), (0.9421, 0.5683), (0.9304, 0.5728), (0.9096, 0.5961), (0.9004, 0.6217),
        (0.8788, 0.6261), (0.8738, 0.6206), (0.8646, 0.6006), (0.8688, 0.5828), (0.8588, 0.5717), (0.8612, 0.5483),
        (0.8471, 0.5139), (0.8496, 0.4906), (0.8338, 0.4772), (0.8362, 0.4628), (0.8221, 0.4150), (0.7946, 0.3794),
        (0.7696, 0.3606), (0.7712, 0.3461), (0.7579, 0.3239), (0.7496, 0.2672), (0.7104, 0.2306), (0.7104, 0.2139),
        (0.6962, 0.2061), (0.6888, 0.1894), (0.6771, 0.1828), (0.6771, 0.1306),
    ],
    "Millis": [
        (0.8588, 0.6406), (0.8904, 0.6572), (0.9138, 0.6906), (0.9246, 0.6928), (0.9471, 0.7150), (0.9546, 0.7672),
        (0.9429, 0.7772), (0.9404, 0.7961), (0.9312, 0.8128), (0.9162, 0.8172), (0.9088, 0.8339), (0.8838, 0.8394),
        (0.8779, 0.8528), (0.8654, 0.8483), (0.8421, 0.8661), (0.8154, 0.8728), (0.8071, 0.8839), (0.7629, 0.9028),
        (0.7488, 0.8994), (0.7412, 0.9094), (0.7304, 0.9050), (0.7196, 0.9117), (0.6912, 0.9117), (0.6838, 0.9039),
        (0.6721, 0.9128), (0.6388, 0.9028), (0.6204, 0.8972), (0.6112, 0.9039), (0.5821, 0.8872), (0.5712, 0.8950),
        (0.5571, 0.8939), (0.5579, 0.8828), (0.5721, 0.8772), (0.5700, 0.8680), (0.5988, 0.8794), (0.6354, 0.8517),
        (0.6587, 0.8517), (0.6838, 0.8417), (0.6988, 0.8217), (0.6988, 0.8072), (0.7146, 0.7894), (0.7121, 0.7672),
        (0.7421, 0.7494), (0.7579, 0.7606), (0.7846, 0.7494), (0.7896, 0.7383), (0.8146, 0.7228), (0.8246, 0.7094),
        (0.8262, 0.6939), (0.8488, 0.6750), (0.8488, 0.6494),
    ],
    "Begaritt": [
        (0.0304, 0.6117), (0.0696, 0.6261), (0.0829, 0.6383), (0.0950, 0.6490), (0.1060, 0.6640), (0.1162, 0.6861),
        (0.1371, 0.7072), (0.1421, 0.7406), (0.1704, 0.7872), (0.1671, 0.8194), (0.1904, 0.8428), (0.2146, 0.8472),
        (0.2429, 0.8661), (0.2546, 0.8606), (0.2612, 0.8728), (0.2729, 0.8683), (0.2921, 0.8883), (0.3304, 0.8872),
        (0.3729, 0.9094), (0.3738, 0.9228), (0.3646, 0.9228), (0.3396, 0.9420), (0.3271, 0.9430), (0.3196, 0.9570),
        (0.3054, 0.9545), (0.2838, 0.9670), (0.2729, 0.9620), (0.2429, 0.9735), (0.2312, 0.9690), (0.2154, 0.9740),
        (0.2046, 0.9680), (0.1796, 0.9680), (0.1721, 0.9620), (0.1446, 0.9590), (0.1321, 0.9540), (0.1238, 0.9420),
        (0.1038, 0.9400), (0.0862, 0.9083), (0.0754, 0.8983), (0.0721, 0.8717), (0.0321, 0.7861), (0.0512, 0.7506),
        (0.0230, 0.7117), (0.0250, 0.6894), (0.0404, 0.6761), (0.0279, 0.6417), (0.0254, 0.6194),
    ],
}
CONTINENT_IDS = {"Central": 1, "Heaven": 2, "Demon": 3, "Millis": 4, "Begaritt": 5, "Islands": 6}

# Island chains (small outlines). Group names are used for descriptions only.
ISLANDS = [
    # between the Heaven Continent's east cape and the Demon Continent
    ("SkyReach", [(0.6741, 0.0328), (0.6824, 0.0417), (0.6916, 0.0394), (0.6974, 0.0328), (0.7008, 0.0372),
                  (0.6932, 0.0494), (0.6958, 0.0717), (0.7066, 0.0750), (0.7174, 0.0883), (0.7116, 0.0928),
                  (0.7008, 0.0817), (0.6949, 0.0917), (0.6916, 0.0794), (0.6774, 0.0683), (0.6774, 0.0550),
                  (0.6707, 0.0417)]),
    ("SkyReach", [(0.7191, 0.0406), (0.7291, 0.0406), (0.7332, 0.0490), (0.7249, 0.0606), (0.7274, 0.0739),
                  (0.7216, 0.0728), (0.7182, 0.0583), (0.7174, 0.0428)]),
    ("SkyReach", [(0.7449, 0.0606), (0.7491, 0.0617), (0.7516, 0.0772), (0.7491, 0.0861), (0.7416, 0.0783)]),
    # west of Wenport
    ("Wenshoals", [(0.8262, 0.5261), (0.8338, 0.5383), (0.8462, 0.5394), (0.8504, 0.5461), (0.8404, 0.5572),
                   (0.8404, 0.5717), (0.8496, 0.5794), (0.8496, 0.5872), (0.8438, 0.5906), (0.8321, 0.5828),
                   (0.8329, 0.5572), (0.8196, 0.5528), (0.8238, 0.5428)]),
    ("Wenshoals", [(0.8071, 0.5306), (0.8146, 0.5361), (0.8104, 0.5506), (0.8046, 0.5550), (0.8012, 0.5483)]),
    ("Wenshoals", [(0.7829, 0.5339), (0.7904, 0.5372), (0.7871, 0.5483), (0.7796, 0.5461)]),
    ("Wenshoals", [(0.8079, 0.5028), (0.8154, 0.5139), (0.8088, 0.5183), (0.8038, 0.5083)]),
    ("Wenshoals", [(0.8196, 0.5628), (0.8254, 0.5683), (0.8221, 0.5772), (0.8154, 0.5706)]),
    ("Wenshoals", [(0.7696, 0.5528), (0.7771, 0.5572), (0.7738, 0.5650), (0.7662, 0.5606)]),
    ("Wenshoals", [(0.7904, 0.5594), (0.7954, 0.5639), (0.7938, 0.5706), (0.7879, 0.5661)]),
    # the strait between the Central tail (East Port) and Millis (West Port)
    ("PortStrait", [(0.5504, 0.7994), (0.5588, 0.8083), (0.5629, 0.8106), (0.5638, 0.8217), (0.5738, 0.8261),
                    (0.5771, 0.8328), (0.5712, 0.8406), (0.5588, 0.8394), (0.5479, 0.8250), (0.5454, 0.8061)]),
    ("PortStrait", [(0.5662, 0.7872), (0.5712, 0.7961), (0.5779, 0.8028), (0.5788, 0.8106), (0.5721, 0.8128),
                    (0.5638, 0.7961)]),
    ("PortStrait", [(0.5962, 0.8261), (0.6021, 0.8383), (0.5946, 0.8494), (0.5879, 0.8406), (0.5938, 0.8294)]),
    ("PortStrait", [(0.5306, 0.8001), (0.5406, 0.8090), (0.5456, 0.8157), (0.5439, 0.8223), (0.5389, 0.8223),
                    (0.5298, 0.8101)]),
    ("PortStrait", [(0.6254, 0.8239), (0.6304, 0.8317), (0.6212, 0.8383), (0.6146, 0.8417), (0.6129, 0.8350)]),
    ("PortStrait", [(0.5879, 0.7939), (0.5954, 0.7983), (0.5913, 0.8050), (0.5838, 0.8006)]),
    ("PortStrait", [(0.5546, 0.8450), (0.5612, 0.8506), (0.5588, 0.8572), (0.5521, 0.8539)]),
    ("PortStrait", [(0.6104, 0.8106), (0.6162, 0.8183), (0.6104, 0.8217), (0.6079, 0.8139)]),
    ("PortStrait", [(0.5071, 0.7928), (0.5129, 0.7983), (0.5088, 0.8028), (0.5054, 0.7950)]),
    ("PortStrait", [(0.5212, 0.7883), (0.5262, 0.7906), (0.5229, 0.7972), (0.5196, 0.7894)]),
    ("PortStrait", [(0.5421, 0.7828), (0.5462, 0.7839), (0.5454, 0.7906), (0.5404, 0.7894)]),
    # south of the Central tail
    ("KingsCays", [(0.4229, 0.8983), (0.4321, 0.8983), (0.4404, 0.9128), (0.4379, 0.9172), (0.4179, 0.9150),
                   (0.4096, 0.9139), (0.4104, 0.9083)]),
    ("KingsCays", [(0.4179, 0.8839), (0.4229, 0.8928), (0.4138, 0.8950), (0.4062, 0.8994), (0.4054, 0.8928)]),
    ("KingsCays", [(0.3996, 0.8683), (0.4054, 0.8728), (0.4012, 0.8839), (0.3954, 0.8794)]),
    ("KingsCays", [(0.4462, 0.8828), (0.4521, 0.8872), (0.4488, 0.8950), (0.4429, 0.8917)]),
    ("KingsCays", [(0.4271, 0.8583), (0.4321, 0.8606), (0.4288, 0.8694), (0.4229, 0.8672)]),
    ("KingsCays", [(0.4071, 0.8450), (0.4138, 0.8506), (0.4112, 0.8572), (0.4046, 0.8528)]),
    ("KingsCays", [(0.3979, 0.9106), (0.4029, 0.9128), (0.4004, 0.9194), (0.3954, 0.9150)]),
    ("KingsCays", [(0.4496, 0.8690), (0.4546, 0.8712), (0.4512, 0.8790), (0.4480, 0.8760)]),
    ("KingsCays", [(0.4679, 0.8723), (0.4729, 0.8746), (0.4704, 0.8801), (0.4654, 0.8779)]),
    # west of the Lower Jaw, between the Central Continent and Begaritt
    ("JawIsles", [(0.1296, 0.6072), (0.1371, 0.6150), (0.1304, 0.6317), (0.1246, 0.6339), (0.1162, 0.6217)]),
    ("JawIsles", [(0.1229, 0.5761), (0.1288, 0.5828), (0.1238, 0.6006), (0.1179, 0.5939), (0.1179, 0.5817)]),
    ("JawIsles", [(0.1438, 0.6306), (0.1529, 0.6372), (0.1471, 0.6472), (0.1388, 0.6417)]),
    ("JawIsles", [(0.1396, 0.5561), (0.1462, 0.5617), (0.1412, 0.5728), (0.1362, 0.5650)]),
    ("JawIsles", [(0.1254, 0.6539), (0.1312, 0.6594), (0.1279, 0.6683), (0.1204, 0.6617)]),
    ("JawIsles", [(0.1462, 0.5817), (0.1546, 0.5850), (0.1512, 0.5917), (0.1429, 0.5880)]),
    ("JawIsles", [(0.1612, 0.6172), (0.1679, 0.6250), (0.1646, 0.6306), (0.1571, 0.6228)]),
    ("JawIsles", [(0.1004, 0.6183), (0.1062, 0.6217), (0.1046, 0.6317), (0.0988, 0.6230)]),
    ("JawIsles", [(0.1696, 0.5939), (0.1737, 0.5990), (0.1704, 0.6039), (0.1662, 0.5990)]),
    # off the north-east coast of the Northern Territories
    ("FrostChain", [(0.4721, 0.2039), (0.4888, 0.2094), (0.4912, 0.2150), (0.4779, 0.2328), (0.4729, 0.2139)]),
    ("FrostChain", [(0.4562, 0.2461), (0.4596, 0.2500), (0.4479, 0.2650), (0.4346, 0.2761), (0.4296, 0.2717),
                    (0.4396, 0.2583), (0.4521, 0.2539)]),
    ("FrostChain", [(0.4438, 0.2083), (0.4512, 0.2172), (0.4596, 0.2206), (0.4454, 0.2261), (0.4421, 0.2139)]),
    ("FrostChain", [(0.5088, 0.1517), (0.5121, 0.1539), (0.5062, 0.1706), (0.4996, 0.1717), (0.4996, 0.1617)]),
    ("FrostChain", [(0.4879, 0.1761), (0.4912, 0.1817), (0.4838, 0.1961), (0.4804, 0.1894)]),
    ("FrostChain", [(0.4062, 0.2506), (0.4212, 0.2517), (0.4221, 0.2561), (0.4079, 0.2594)]),
    ("FrostChain", [(0.5112, 0.1261), (0.5171, 0.1330), (0.5138, 0.1361), (0.5071, 0.1310)]),
    ("FrostChain", [(0.4679, 0.1750), (0.4721, 0.1780), (0.4688, 0.1861), (0.4654, 0.1850)]),
]

# ------------------------------------------------------------------------------------------ region partitions
# Central Continent sub-regions, evaluated in this order at noise-warped coordinates; the Red Wyrm body (id 3) is
# taken from the range mask. Anything left on the Central Continent is Southern Central (6).
NORTH_POLY = [(0.0, 0.0), (0.5, 0.0), (0.5, 0.30), (0.39, 0.29), (0.36, 0.26), (0.33, 0.245), (0.30, 0.235),
              (0.27, 0.232), (0.25, 0.23), (0.22, 0.226), (0.19, 0.222), (0.16, 0.215), (0.13, 0.17), (0.11, 0.142),
              (0.085, 0.155), (0.068, 0.163), (0.055, 0.168), (0.04, 0.161), (0.0, 0.155)]
FITTOA_POLY = [(0.0, 0.155), (0.04, 0.161), (0.055, 0.168), (0.068, 0.163), (0.085, 0.155), (0.11, 0.142),
               (0.13, 0.17), (0.15, 0.20), (0.17, 0.235), (0.183, 0.265), (0.19, 0.28), (0.14, 0.284), (0.10, 0.270),
               (0.06, 0.266), (0.0, 0.266)]
ASURA_POLY = [(0.0, 0.266), (0.06, 0.266), (0.10, 0.270), (0.14, 0.284), (0.19, 0.28), (0.20, 0.36), (0.21, 0.45),
              (0.22, 0.55), (0.2165, 0.578), (0.208, 0.595), (0.19, 0.64), (0.0, 0.64)]
STRIFE_POLY = [(0.16, 0.215), (0.19, 0.222), (0.22, 0.226), (0.25, 0.23), (0.27, 0.232), (0.30, 0.235),
               (0.33, 0.245), (0.36, 0.26), (0.39, 0.29), (0.5, 0.30), (0.5, 0.66), (0.40, 0.65), (0.34, 0.62),
               (0.30, 0.60), (0.26, 0.585), (0.2165, 0.578), (0.22, 0.55), (0.21, 0.45), (0.20, 0.36), (0.19, 0.28),
               (0.17, 0.235)]
GREAT_FOREST_POLY = [(0.80, 0.745), (0.815, 0.715), (0.835, 0.690), (0.870, 0.672), (0.960, 0.700), (0.970, 0.780),
                     (0.920, 0.790), (0.880, 0.785), (0.850, 0.775), (0.820, 0.765)]
BEGARITT_BADLANDS_POLY = [(0.0, 0.58), (0.20, 0.58), (0.16, 0.70), (0.178, 0.76), (0.19, 0.82), (0.15, 0.85),
                          (0.10, 0.862), (0.05, 0.85), (0.0, 0.84)]

# ------------------------------------------------------------------------------------------ mountain ranges
# Crest polylines: (u, v, crest height m). core = half-width of the steep body (m), foot = foothill reach (m).
RANGES = [
    dict(name="RedWyrm", region=3, core=720.0, foot=1250.0, foot_h=230.0, jag=1.0, snow=550.0, lines=[
        [(0.030, 0.158, 297, 0.5), (0.040, 0.161, 270, 0.5), (0.055, 0.168, 135, 0.5), (0.068, 0.163, 387, 0.55),
         (0.085, 0.155, 585, 0.6), (0.110, 0.142, 693, 0.62)],
        [(0.110, 0.142, 693, 0.62), (0.130, 0.170, 774, 0.7), (0.150, 0.200, 801, 0.72), (0.172, 0.236, 756, 0.7),
         (0.192, 0.282, 720, 0.78), (0.200, 0.360, 783, 0.95), (0.210, 0.450, 720, 1.0), (0.220, 0.550, 594, 0.8),
         (0.2165, 0.578, 135, 0.55), (0.208, 0.595, 324, 0.45)],
        [(0.160, 0.217, 729, 0.75), (0.190, 0.222, 747, 0.8), (0.220, 0.226, 765, 0.85), (0.250, 0.230, 774, 0.85),
         (0.270, 0.232, 837, 0.9), (0.300, 0.235, 765, 0.85), (0.330, 0.245, 711, 0.8), (0.360, 0.260, 648, 0.75),
         (0.390, 0.290, 486, 0.62)],
    ]),
    dict(name="KingDragon", region=6, core=660.0, foot=1150.0, foot_h=160.0, jag=1.0, snow=600.0, lines=[
        [(0.280, 0.565, 353, 0.7), (0.290, 0.580, 484, 0.85), (0.305, 0.615, 595, 1.0), (0.320, 0.650, 651, 1.0),
         (0.335, 0.685, 614, 1.0), (0.350, 0.720, 558, 0.9), (0.365, 0.745, 391, 0.7)],
    ]),
    dict(name="BlueWyrm", region=12, core=700.0, foot=1200.0, foot_h=170.0, jag=1.0, snow=600.0, lines=[
        [(0.797, 0.766, 334, 0.7), (0.820, 0.780, 537, 0.9), (0.850, 0.800, 669, 1.0), (0.875, 0.803, 616, 1.0),
         (0.900, 0.800, 546, 0.9), (0.918, 0.794, 334, 0.7)],
    ]),
    dict(name="Bashelant", region=4, core=540.0, foot=900.0, foot_h=120.0, jag=0.9, snow=450.0, lines=[
        [(0.180, 0.080, 380, 0.9), (0.195, 0.074, 630, 1.0), (0.212, 0.090, 430, 0.9)],
    ]),
    dict(name="NorthSpur", region=4, core=500.0, foot=880.0, foot_h=110.0, jag=0.9, snow=450.0, lines=[
        [(0.313, 0.197, 420, 1.0), (0.330, 0.190, 570, 1.0), (0.346, 0.201, 400, 1.0)],
    ]),
    dict(name="DemonCragsNorth", region=8, core=300.0, foot=650.0, foot_h=90.0, jag=1.6, snow=9999.0, lines=[
        [(0.815, 0.150, 323, 1.0), (0.835, 0.168, 476, 1.0), (0.848, 0.188, 544, 1.0), (0.852, 0.212, 442, 1.0),
         (0.860, 0.240, 323, 1.0)],
    ]),
    dict(name="DemonCragsCentre", region=8, core=340.0, foot=750.0, foot_h=100.0, jag=1.6, snow=9999.0, lines=[
        [(0.800, 0.300, 323, 1.0), (0.820, 0.316, 476, 1.0), (0.845, 0.335, 552, 1.0), (0.860, 0.356, 510, 1.0),
         (0.870, 0.380, 408, 1.0), (0.866, 0.402, 289, 1.0)],
        [(0.834, 0.360, 357, 1.0), (0.812, 0.382, 306, 1.0)],
    ]),
]

# Passes through the Red Wyrm range: (u, v), crossing direction (degrees, 0 = east, 90 = south), saddle height m.
PASSES = {
    "UpperJaw": dict(uv=(0.055, 0.168), cross_deg=95.0, saddle=165.0, radius=430.0, span=1300.0),
    "LowerJaw": dict(uv=(0.2165, 0.578), cross_deg=12.0, saddle=175.0, radius=450.0, span=1500.0),
}

# Graded pass corridors (applied after erosion): approach -> col -> approach, saddle height (m), max grade.
PASS_CORRIDORS = {
    "UpperJaw": dict(pts=[(0.090, 0.224), (0.056, 0.165), (0.078, 0.120)], saddle=120.0, grade=0.085),
    "LowerJaw": dict(pts=[(0.166, 0.536), (0.2165, 0.578), (0.250, 0.586)], saddle=120.0, grade=0.085),
    "RingGate": dict(pts=[(0.1030, 0.7956), (0.1215, 0.8162), (0.1316, 0.8565)], saddle=97.0, grade=0.07, core=22.0, blend=120.0),
}

# ------------------------------------------------------------------------------------------ craters, ring, outcrops
CRATERS = [
    # (name, u, v, rim radius m, rim height m, floor depth m below surroundings)
    ("Rikarisu", 0.885, 0.205, 560.0, 90.0, 55.0),
    ("Cinder", 0.780, 0.200, 150.0, 30.0, 18.0),
    ("Ashbowl", 0.930, 0.330, 220.0, 45.0, 25.0),
    ("Scorch", 0.790, 0.440, 130.0, 28.0, 16.0),
    ("Hollow", 0.900, 0.460, 180.0, 36.0, 22.0),
    ("Pit", 0.860, 0.530, 120.0, 25.0, 14.0),
    ("Blister", 0.740, 0.135, 100.0, 22.0, 12.0),
    ("Maw", 0.940, 0.400, 160.0, 32.0, 20.0),
    ("Ember", 0.890, 0.285, 140.0, 30.0, 18.0),
]
CALDERA = dict(name="Caldera", uv=(0.830, 0.272), rim_r=470.0, rim_h=120.0)
BEGARITT_RING = dict(uv=(0.098, 0.790), r_in=330.0, r_out=720.0, h_lo=300.0, h_hi=500.0, floor=95.0,
                     gap_deg=40.0)
OUTCROPS = [
    # (u, v, radius m, height m): isolated sandstone buttes (labyrinth gates can be set into them)
    (0.242, 0.887, 240.0, 190.0),
    (0.122, 0.915, 200.0, 150.0),
    (0.215, 0.935, 150.0, 110.0),
]

# ------------------------------------------------------------------------------------------ rivers and lakes
# River guides (source -> mouth). The generator pre-carves a valley along each guide, erodes, then re-traces the real
# channel by steepest descent, so the final centrelines follow the eroded valley floors.
RIVERS = [
    dict(name="Fittoa River", key="FittoaRiver", continent="Central", width=(6.0, 26.0), pts=[
        (0.158, 0.232), (0.146, 0.238), (0.130, 0.242), (0.112, 0.244), (0.092, 0.246), (0.072, 0.250),
        (0.054, 0.254), (0.034, 0.258)]),
    dict(name="Ars River", key="ArsRiver", continent="Central", width=(7.0, 34.0), pts=[
        (0.188, 0.425), (0.170, 0.405), (0.150, 0.385), (0.128, 0.370), (0.105, 0.362), (0.085, 0.366),
        (0.066, 0.370)]),
    dict(name="Wishill Brook", key="WishillBrook", continent="Central", width=(5.0, 18.0), pts=[
        (0.203, 0.500), (0.182, 0.505), (0.162, 0.514), (0.139, 0.521)]),
    dict(name="Strife River", key="StrifeRiver", continent="Central", width=(6.0, 26.0), pts=[
        (0.290, 0.258), (0.300, 0.290), (0.318, 0.325), (0.338, 0.355), (0.362, 0.378)]),
    dict(name="Kings River", key="KingsRiver", continent="Central", width=(5.0, 22.0), pts=[
        (0.330, 0.612), (0.350, 0.626), (0.372, 0.640), (0.391, 0.642)]),
    dict(name="Jungle River", key="JungleRiver", continent="Central", width=(6.0, 26.0), pts=[
        (0.302, 0.662), (0.285, 0.676), (0.266, 0.672), (0.246, 0.668)]),
    dict(name="Frost River", key="FrostRiver", continent="Central", width=(5.0, 20.0), pts=[
        (0.226, 0.205), (0.234, 0.186), (0.244, 0.172), (0.249, 0.168)], lake_out="FrostLake"),
    dict(name="Frost Outflow", key="FrostOutflow", continent="Central", width=(8.0, 20.0), pts=[
        (0.257, 0.151), (0.259, 0.136), (0.263, 0.117)], lake_in="FrostLake"),
    dict(name="Ash River", key="AshRiver", continent="Demon", width=(6.0, 20.0), pts=[
        (0.812, 0.272), (0.795, 0.270), (0.775, 0.278), (0.750, 0.282)], lake_in="CalderaLake"),
    dict(name="Rust River", key="RustRiver", continent="Demon", width=(5.0, 20.0), pts=[
        (0.872, 0.420), (0.888, 0.468), (0.905, 0.518), (0.925, 0.552), (0.944, 0.566)]),
    dict(name="Nicolas River", key="NicolasRiver", continent="Millis", width=(6.0, 28.0), pts=[
        (0.832, 0.815), (0.812, 0.840), (0.795, 0.868), (0.786, 0.896)]),
    dict(name="Holy Lake Inflow", key="LakeInflow", continent="Millis", width=(5.0, 16.0), pts=[
        (0.800, 0.792), (0.790, 0.812), (0.780, 0.832)], lake_out="MillishionLake"),
    dict(name="Holy Lake Outflow", key="LakeOutflow", continent="Millis", width=(10.0, 24.0), pts=[
        (0.764, 0.866), (0.758, 0.882), (0.752, 0.902)], lake_in="MillishionLake"),
    dict(name="Forest River", key="ForestRiver", continent="Millis", width=(6.0, 26.0), pts=[
        (0.900, 0.785), (0.902, 0.752), (0.892, 0.720), (0.882, 0.690), (0.875, 0.648)]),
]

LAKES = [
    # key, name, (u, v), (semi-axis a, b m), angle deg, frozen, kind
    dict(key="MillishionLake", name="Holy Lake", uv=(0.768, 0.848), ab=(340.0, 210.0), ang=20.0, frozen=False,
         kind="Lake"),
    dict(key="SerenLake", name="Lake Seren", uv=(0.714, 0.879), ab=(230.0, 130.0), ang=-10.0, frozen=False,
         kind="Lake"),
    dict(key="ForestMere", name="Forest Mere", uv=(0.905, 0.735), ab=(170.0, 110.0), ang=30.0, frozen=False,
         kind="Lake"),
    dict(key="CalderaLake", name="Caldera Lake", uv=(0.830, 0.272), ab=(270.0, 230.0), ang=0.0, frozen=False,
         kind="CraterLake"),
    dict(key="FrostLake", name="Frost Lake", uv=(0.254, 0.160), ab=(240.0, 140.0), ang=-25.0, frozen=True,
         kind="Lake"),
    dict(key="MirrorLake", name="Mirror Lake", uv=(0.380, 0.205), ab=(200.0, 120.0), ang=25.0, frozen=True,
         kind="Lake"),
    dict(key="TarnSharia", name="Sharia Tarn", uv=(0.168, 0.118), ab=(150.0, 95.0), ang=20.0, frozen=True,
         kind="Lake"),
    dict(key="JungleLake", name="Emerald Pool", uv=(0.262, 0.640), ab=(180.0, 120.0), ang=-30.0, frozen=False,
         kind="Lake"),
    dict(key="OasisWest", name="Oasis of Palms", uv=(0.205, 0.915), ab=(48.0, 34.0), ang=0.0, frozen=False,
         kind="Oasis"),
    dict(key="OasisNorth", name="Caravan Well", uv=(0.145, 0.862), ab=(40.0, 30.0), ang=20.0, frozen=False,
         kind="Oasis"),
    dict(key="OasisRapan", name="Rapan Spring", uv=(0.262, 0.934), ab=(52.0, 36.0), ang=-20.0, frozen=False,
         kind="Oasis"),
]

# ------------------------------------------------------------------------------------------ sites (Spec section 4)
# target (u, v), radius m, placement mode: "inland" (flat land, full footprint on land), "port" (on the coast),
# "fixed" (terrain is designed around the given point), "gate" (set into the nearest rock face)
SITES = [
    dict(id="Ars", name="Ars", region=1, uv=(0.085, 0.310), r=450.0, style="AsuraCapital", mode="inland",
         search=900.0),
    dict(id="Roa", name="Roa", region=2, uv=(0.098, 0.222), r=190.0, style="AsuraTown", mode="inland", search=700.0),
    dict(id="Buena", name="Buena Village", region=2, uv=(0.136, 0.258), r=130.0, style="RuralVillage", mode="inland",
         search=600.0),
    dict(id="Sharia", name="Sharia", region=4, uv=(0.138, 0.100), r=300.0, style="NorthernCity", mode="inland",
         search=700.0),
    dict(id="Rikarisu", name="Rikarisu", region=9, uv=(0.885, 0.205), r=220.0, style="DemonTown", mode="fixed",
         search=0.0),
    dict(id="Wenport", name="Wenport", region=8, uv=(0.915, 0.590), r=140.0, style="DemonPort", mode="port",
         search=900.0),
    dict(id="ZantPort", name="Zant Port", region=11, uv=(0.845, 0.665), r=130.0, style="MillisPort", mode="port",
         search=900.0),
    dict(id="Millishion", name="Millishion", region=11, uv=(0.757, 0.797), r=420.0, style="MillisCapital",
         mode="fixed", search=0.0),
    dict(id="WestPort", name="West Port", region=11, uv=(0.568, 0.884), r=120.0, style="MillisPort", mode="port",
         search=700.0),
    dict(id="EastPort", name="East Port", region=6, uv=(0.527, 0.847), r=120.0, style="AsuraTown", mode="port",
         search=700.0),
    dict(id="KingDragon", name="King Dragon Kingdom capital", region=6, uv=(0.495, 0.842), r=220.0,
         style="AsuraTown", mode="inland", search=500.0),
    dict(id="Rapan", name="Rapan", region=13, uv=(0.300, 0.925), r=280.0, style="DesertCity", mode="inland",
         search=600.0),
    dict(id="Labyrinth_1", name="Ring Labyrinth Gate", region=14, uv=(0.098, 0.768), r=40.0, style="LabyrinthGate",
         mode="gate", search=500.0),
    dict(id="Labyrinth_2", name="Northern Mesa Labyrinth Gate", region=14, uv=(0.062, 0.670), r=40.0,
         style="LabyrinthGate", mode="gate", search=700.0),
    dict(id="Labyrinth_3", name="Canyon Labyrinth Gate", region=14, uv=(0.135, 0.735), r=40.0,
         style="LabyrinthGate", mode="gate", search=700.0),
    dict(id="Labyrinth_4", name="Rapan Butte Labyrinth Gate", region=13, uv=(0.242, 0.887), r=40.0,
         style="LabyrinthGate", mode="gate", search=500.0),
    dict(id="Labyrinth_5", name="Southern Butte Labyrinth Gate", region=13, uv=(0.122, 0.915), r=40.0,
         style="LabyrinthGate", mode="gate", search=500.0),
    dict(id="SwordSanctuary", name="Sword Sanctuary", region=4, uv=(0.030, 0.095), r=100.0,
         style="NorthernVillage", mode="inland", search=700.0),
]

# ------------------------------------------------------------------------------------------ roads (Spec section 5)
# Each road: list of stops (site ids or pass/waypoint keys). Built in order; later roads reuse earlier ones.
WAYPOINTS = {
    "UpperJaw": (0.056, 0.165),
    "LowerJaw": (0.2165, 0.578),
    "StrifeSouth": (0.262, 0.546),
    "KikkaPlain": (0.345, 0.590),
    "KingsCoast": (0.366, 0.655),
    "TailNorth": (0.420, 0.770),
    "GreatForestHeart": (0.838, 0.722),
    "RapanHarbour": (0.335, 0.890),
}
ROADS = [
    dict(key="KingsHighway", name="Kings Highway", stops=["Ars", "Roa"], width=9.0, klass="Highway"),
    dict(key="FittoaLane", name="Fittoa Lane", stops=["Roa", "Buena"], width=6.0, klass="Road"),
    dict(key="NorthRoad", name="Upper Jaw Road", stops=["Roa", "UpperJaw", "Sharia"], width=7.0, klass="Road"),
    dict(key="SanctuaryTrail", name="Sanctuary Trail", stops=["Sharia", "SwordSanctuary"], width=5.0, klass="Trail"),
    dict(key="SouthRoad", name="Lower Jaw Road", stops=["Ars", "LowerJaw", "StrifeSouth", "KikkaPlain", "KingsCoast",
                                                        "TailNorth", "KingDragon", "EastPort"], width=8.0,
         klass="Highway"),
    dict(key="HolyRoad", name="West Port Road", stops=["WestPort", "Millishion"], width=8.0, klass="Highway"),
    dict(key="HolySwordHighway", name="Holy Sword Highway", stops=["Millishion", "GreatForestHeart", "ZantPort"],
         width=9.0, klass="Highway"),
    dict(key="CraterRoad", name="Crater Road", stops=["Wenport", "Rikarisu"], width=7.0, klass="Road"),
    dict(key="HarbourRoad", name="Rapan Harbour Road", stops=["Rapan", "RapanHarbour"], width=7.0, klass="Road"),
    dict(key="Gate4Road", name="Butte Gate Track", stops=["Rapan", "Labyrinth_4"], width=5.0, klass="Trail"),
    dict(key="Gate5Road", name="Southern Gate Track", stops=["Rapan", "Labyrinth_5"], width=5.0, klass="Trail"),
    dict(key="Gate1Road", name="Ring Gate Track", stops=["Labyrinth_5", "Labyrinth_1"], width=5.0, klass="Trail"),
    dict(key="Gate3Road", name="Canyon Gate Track", stops=["Labyrinth_1", "Labyrinth_3"], width=5.0, klass="Trail"),
    dict(key="Gate2Road", name="Mesa Gate Track", stops=["Labyrinth_3", "Labyrinth_2"], width=5.0, klass="Trail"),
]
