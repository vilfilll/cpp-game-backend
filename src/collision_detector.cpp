#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;

    for (size_t g_id = 0; g_id < provider.GatherersCount(); ++g_id) {
        const auto gatherer = provider.GetGatherer(g_id);

        if (gatherer.start_pos.x == gatherer.end_pos.x && gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        for (size_t i_id = 0; i_id < provider.ItemsCount(); ++i_id) {
            const auto item = provider.GetItem(i_id);
            const double collect_radius = gatherer.width + item.width;
            const auto result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            if (result.IsCollected(collect_radius)) {
                events.push_back(GatheringEvent{
                    .item_id = i_id,
                    .gatherer_id = g_id,
                    .sq_distance = result.sq_distance,
                    .time = result.proj_ratio
                });
            }
        }
    }

    std::sort(events.begin(), events.end(),
              [](const GatheringEvent& a, const GatheringEvent& b) { return a.time < b.time; });
    return events;
}

}  // namespace collision_detector